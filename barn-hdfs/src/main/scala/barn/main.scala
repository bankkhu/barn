package barn

import Stream.continually
import placement._

import org.apache.hadoop.conf.{Configuration => HadoopConf}
import org.joda.time._

object BarnHdfsWriter
  extends App
  with Logging
  with Hadoop
  with SvlogdFile
  with FileCombiner
  with HdfsPlacementStrategy
  with LocalPlacementStrategy
  with ParamParser
  with TimeUtils
  with Instruments {

  val minMB = 1 //minimum megabytes to keep for each service!
  val maxLookBackDays = 3
  val maxReadySize = 1 * 1024 * 1024 * 1024 // 1GB
  val excludeList = List("""^\..*""") //Exclude files starting with dot (temp)

  loadConf(args) { barnConf => {

    enableGanglia(barnConf.appName
               , GangliaOpts(barnConf.gangliaHost
                           , barnConf.gangliaPort))

    enableJMX()

    continually(() => listSubdirectories(barnConf.localLogDir)).iterator
      .foreach { listDirs => {

        info("Round of sync started.")

        listDirs() match {
          case Left(err) => logBarnError(s"List dirs in ${barnConf.localLogDir}")(err)
          case Right(lst) => syncRootLogDir(barnConf)(lst)
        }

        info("Round of sync finished.")
      }
    }
  }}

  def syncRootLogDir(barnConf: BarnConf)(dirs: List[Dir])
  : Unit = dirs match {
    case Nil =>
      info("No service has appeared in root log dir. Incorporating patience.")
      Thread.sleep(1000)
    case xs =>

     import scala.collection.JavaConverters._
     val hdfsListCache : HdfsListCache = new HdfsListCacheJ asScala

     //Working around SI-4843 https://issues.scala-lang.org/browse/SI-4843
     if(barnConf.runParallel) {
        import scala.collection.parallel._
        import scala.concurrent._

        val xsPar = xs.par
        xsPar.tasksupport = new ForkJoinTaskSupport(
                              new forkjoin.ForkJoinPool(barnConf.degParallel))

        xsPar map actOnServiceDir(barnConf, hdfsListCache)
      } else
        xs map actOnServiceDir(barnConf, hdfsListCache)
  }

  def actOnServiceDir(barnConf: BarnConf, hdfsListCache: HdfsListCache)
                     (serviceDir : Dir) = {

    reportOngoingSync {

    val result = for {
      serviceInfo <- decodeServiceInfo(serviceDir).right
      fs          <- createLazyFileSystem(barnConf.hdfsEndpoint).right
      localFiles  <- listSortedLocalFiles(serviceDir, excludeList).right
      totalReadySize <- Right(sumFileSizes(localFiles)).right
      lookBack    <- earliestLookbackDate(localFiles, maxLookBackDays).right
      plan        <- planNextShip(fs
                                , serviceInfo
                                , totalReadySize
                                , maxReadySize
                                , barnConf.hdfsLogDir
                                , barnConf.shipInterval
                                , lookBack
                                , hdfsListCache).right

      candidates  <- outstandingFiles(localFiles, plan lastTaistamp, maxLookBackDays).right
      concatted   <- reportCombineTime(
                       concatCandidates(candidates, barnConf.localTempDir)).right

      lastTaistamp <- Right(svlogdFileNameToTaiString(candidates.last.getName)).right
      targetName_  <- Right(targetName(lastTaistamp, serviceInfo)).right

      _           <- ensureHdfsDir(fs, plan.hdfsDir).right
      _           <- ensureHdfsDir(fs, plan.hdfsTempDir).right

      _           <- reportShipTime(
                      atomicShipToHdfs(fs
                                    , concatted
                                    , plan hdfsDir
                                    , targetName_
                                    , plan hdfsTempDir)).right

      shippedTS   <- svlogdFileTimestamp(candidates last).right
      _           <- cleanupLocal(serviceDir
                                , shippedTS
                                , minMB
                                , excludeList).right
    } yield ()

    result.left.map( _ =>
                     cleanupLocal(serviceDir
                                , DateTime.now.minusDays(maxLookBackDays+1)
                                , minMB
                                , excludeList))

    result.left.map(reportError(s"Sync of $serviceDir") _)

    }
  }

  def reportError(context: String)(e: BarnError) = e match {
    case _ : BarnFatalError => {
      logBarnError(context)(e)
      reportFatalError
    }
    case _ => ()
  }

  def earliestLookbackDate(localFiles: List[File], maxLookBackDays: Int)
  : Either[BarnError, DateTime] = {
    val maxLookBackTime = DateTime.now.minusDays(maxLookBackDays)

    localFiles.headOption match {
      case Some(f) =>
        svlogdFileTimestamp(f).right.map(ts =>
          if(ts.isBefore(maxLookBackTime)) maxLookBackTime else ts)
      case None => Right(maxLookBackTime)
    }
  }

  def outstandingFiles(localFiles: List[File], lastTaistamp: Option[String], maxLookBackTime: Int)
  : Either[BarnError, List[File]] = {
    val maxLookBackTime = DateTime.now.minusDays(maxLookBackDays).minusDays(1).toDateMidnight

    lastTaistamp match {
      case Some(taistamp) =>
        localFiles dropWhile { f =>
            val fileTaistring = svlogdFileNameToTaiString(f getName)

            fileTaistring <= taistamp ||  //TODO Deduplicate me with the case below
              Tai64.convertTai64ToTime(fileTaistring).isBefore(maxLookBackTime)

          } match {
            case Nil => Left(NothingToSync("No local files left to sync."))
            case x   => Right(x)
          }
      case None =>
        localFiles.dropWhile(f =>    //TODO Deduplicate me with the case above
          Tai64.convertTai64ToTime(
            svlogdFileNameToTaiString(f getName))
              .isBefore(maxLookBackTime)) match {
                case Nil => Left(NothingToSync("No local files left to sync."))
                case x   => Right(x)
               }
    }
  }
}



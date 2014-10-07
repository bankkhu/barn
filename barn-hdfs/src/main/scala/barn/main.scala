package barn

import Stream.continually
import placement._

import org.apache.hadoop.conf.{Configuration => HadoopConf}
import org.joda.time._

/** This singleton defines the main barn-hdfs routine which is responsible
  * for looking at list of files it has received from barn-agents, comparing
  * to what is already on HDFS and transfering the difference.
  */

 // TODO: report shipping per item
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

  // Load the barn configuration and launch the synchronization
  // routine on the set of subdirectories.
  loadConf(args) { barnConf => {

    enableGanglia(barnConf.appName
               , GangliaOpts(barnConf.gangliaHost
                           , barnConf.gangliaPort))

    enableJMX();

    continually(() => listSubdirectories(barnConf.localLogDir)).iterator
      .foreach { listDirs => {

        info("Round of sync started.")

        listDirs().fold(logBarnError("List dirs in" + barnConf.localLogDir)
                      , syncRootLogDir(barnConf))

        info("Round of sync finished.")
      }
    }
  }}

  /** This routing here is responsible for launching one
    * or many actOnServiceDir threads
    */
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

 /** Concatenates files it has received from barn-agents
   * and ships them as a single file to HDFS
   *
   * In order to ensure data is sent to HDFS exactly once, this
   * routine decodes timestamps from the filename of the local files and
   * on HDFS. When creating a candidate list of files to concatenate and send
   * to HDFS, it first checks the timestamp of the file available on HDFS.
   * It then filters out local files with timestamps earlier then the file
   * timestamp available on HDFS.
   */
  def actOnServiceDir(barnConf: BarnConf, hdfsListCache: HdfsListCache)
                     (serviceDir : Dir) = {

    reportOngoingSync {

    val result = for {
      serviceInfo <- decodeServiceInfo(serviceDir).right
      fs          <- createLazyFileSystem(barnConf.hdfsEndpoint).right

      // Produce a List[File] of local files sorted by svlogd generated timestamp
      localFiles  <- listSortedLocalFiles(serviceDir, excludeList).right

      // TODO: Measure ready size
      totalReadySize <- Right(sumFileSizes(localFiles)).right

      _ <- PrometheusTelemetry.recordSpool(serviceInfo.serviceName
                                        , "ready"
                                        , totalReadySize)

      // The earliest timestamp of local files or maxLookBack timestamp
      lookBack    <- earliestLookbackDate(localFiles, maxLookBackDays).right

      // Produces a shipping plan which is just a case class containing:
      // hdfsDir, hdfsTempDir, lastTaistamp
      plan        <- planNextShip(fs
                                , serviceInfo
                                , totalReadySize
                                , maxReadySize
                                , barnConf.hdfsLogDir
                                , barnConf.shipInterval
                                , lookBack
                                , hdfsListCache).right
      // candidate files are the local with timestamps earlier then the HDFS lastTaistamp
      // but later then maxLookBackDays
      candidates  <- outstandingFiles(localFiles, plan lastTaistamp, maxLookBackDays).right

      // TODO: measure total candidate size
      _ <- PrometheusTelemetry.recordSpool(serviceInfo.serviceName
                                        , "candidate"
                                        , sumFileSizes(candidates)


      // Concatenate the candidate files into a single file on the local temp directory
      concatted   <-  reportCombineTime(
                       concatCandidates(candidates, barnConf.localTempDir)).right

      // TODO: measure concated (and compressed size)
      _ <- PrometheusTelemetry.recordSpool(serviceInfo.serviceName
                                        , "concatted"
                                        , concatted.length)

      lastTaistamp <- Right(svlogdFileNameToTaiString(candidates.last.getName)).right

      // Generate the filename for the final HDFS destination which includes
      // a taistamp of the latest file to be combined
      targetName_  <- Right(targetName(lastTaistamp, serviceInfo)).right

      // Create the target HDFS final destination directory
      _           <- ensureHdfsDir(fs, plan.hdfsDir).right

      // Create the temporary HDFS destination directory
      _           <- ensureHdfsDir(fs, plan.hdfsTempDir).right

      // Write the local concatenated file to the HDFS temporary
      // directory and then perform an atomic rename of that file
      // to it's final destination
      _           <- reportShipTime(
                      atomicShipToHdfs(fs
                                    , concatted
                                    , plan hdfsDir
                                    , targetName_
                                    , plan hdfsTempDir)).right

      // TODO: Measure shipped concated size
      _ <- PrometheusTelemetry.recordSpool(serviceInfo.serviceName
                                        , "shipped"
                                        , concatted.length)

      // Get the largest timestamp of the files to ship
      shippedTS   <- svlogdFileTimestamp(candidates last).right

      // Delete the local intermediary files
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

    result.left.map(reportError("Sync of " + serviceDir + "") _)

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
        localFiles dropWhile(f => {

            val fileTaistring = svlogdFileNameToTaiString(f getName)

            fileTaistring <= taistamp ||  //TODO Deduplicate me with the case below
              Tai64.convertTai64ToTime(fileTaistring).isBefore(maxLookBackTime)

          }) match {
            case Nil => Left(NothingToSync("No local files left to sync."))
            case x => Right(x)
          }
      case None =>
        localFiles.dropWhile(f =>    //TODO Deduplicate me with the case above
          Tai64.convertTai64ToTime(
            svlogdFileNameToTaiString(f getName))
              .isBefore(maxLookBackTime)) match {
                case Nil => Left(NothingToSync("No local files left to sync."))
                case x => Right(x)
               }
    }
  }
}



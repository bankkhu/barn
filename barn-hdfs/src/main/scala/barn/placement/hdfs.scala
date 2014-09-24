package barn.placement

import org.joda.time._
import scala.math.Ordering
import barn._
import org.apache.hadoop.fs.{FileSystem => HdfsFileSystem}
import TimeUtils._

object HdfsPlacementStrategy extends HdfsPlacementStrategy

trait HdfsPlacementStrategy
  extends Logging
  with Hadoop
  with SvlogdFile {

  implicit def dateBucket2Date(d: DateBucket) : DateTime
  = new DateTime(d.year, d.month, d.day, 0, 0)

  def cachingLs(fs: LazyHdfsFileSystem
              , path: HdfsDir
              , hdfsListCache: HdfsListCache)
  : Either[barn.BarnError, Either[BarnError, List[PlacedFileInfo]]] = {
    hdfsListCache.get(path) match {
      case Some(x) => x
      case None => {
        val listResult = listHdfsFiles(fs, path).right.map(x => collapseValidate(x map getPlacedFileInfo))
        hdfsListCache += ( path -> listResult)
        info(s"I called LS on $path and have a cache of size " + hdfsListCache.size)
        listResult
      }
    }
  }

  def planNextShip(fs: LazyHdfsFileSystem
                 , serviceInfo: LocalServiceInfo
                 , totalReadySize: Long
                 , maxReadySize: Long
                 , baseHdfsDir: HdfsDir
                 , shipInterval: Int
                 , lookBackLowerBound: DateTime
                 , hdfsListCache: HdfsListCache)
  : Either[BarnError, ShippingPlan] = {

    val hdfsTempDir = targetTempDir(baseHdfsDir)
    val hdfsDirStream = targetDirs(DateTime.now, baseHdfsDir, lookBackLowerBound)

    val targetHdfsDir = hdfsDirStream.head

    val dirsWithRelevantHdfsFilesStream: Stream[Either[BarnError, List[PlacedFileInfo]]] = for {
        each           <- hdfsDirStream
        relevantFiles  <- cachingLs(fs, each , hdfsListCache) match {
			      case Left(FileNotFound(_)) => None
			      case Left(a) => Some(Left(a))
          case Right(Left(a)) => Some(Left(a))
			      case Right(Right(Nil)) => None
			      case Right(a) =>
				logsForService(serviceInfo, a) match {
				  case Right(Nil) => None
				  case otherwise => Some(otherwise)
				}
			  }
      } yield relevantFiles

    for {
       hdfsFilesFileInfo    <- dirsWithRelevantHdfsFilesStream
                                .headOption
                                .getOrElse(Right(List.empty[PlacedFileInfo])).right

       lastShippedTaistamp   <- Right(getLastShippedTaistamp(hdfsFilesFileInfo, serviceInfo)).right
       lastShippedTimestamp  <- Right(lastShippedTaistamp.map(Tai64.convertTai64ToTime(_))).right

       _                    <- isShippingTime(lastShippedTimestamp
                                            , shipInterval
                                            , totalReadySize
                                            , maxReadySize).right

    } yield ShippingPlan(targetHdfsDir, hdfsTempDir, lastShippedTaistamp)

  }

  def logsForService(serviceInfo: LocalServiceInfo,
                     hdfsFilesPlacedInfos: Either[BarnError, List[PlacedFileInfo]])
  : Either[BarnError, List[PlacedFileInfo]] = for {
    hdfsFilesPlacedInfos_ <- hdfsFilesPlacedInfos.right
    hdfsFilesFiltered     <- Right(filter(hdfsFilesPlacedInfos_, serviceInfo)).right
  } yield hdfsFilesFiltered

  def getLastShippedTaistamp(hdfsFiles: List[PlacedFileInfo], serviceInfo: LocalServiceInfo)
  : Option[String] = sorted(hdfsFiles, serviceInfo).lastOption.map(_.taistamp)

  def isShippingTime(lastShippedTimestamp: Option[DateTime], shippingInterval: Int, totalReadySize: Long, maxReadySize: Long)
  : Either[BarnError, Unit]
  = lastShippedTimestamp match {
    case Some(lastTimestamp) =>
      (enoughTimePast(lastTimestamp, shippingInterval) ||
       totalReadySize > maxReadySize) match {
        case true => Right(())
        case false => Left(SyncThrottled("I synced not long ago."))
      }
    case None => Right(())
  }

  def targetName(taistamp : String, serviceInfo: LocalServiceInfo) :String = {
    val time = Tai64.convertTai64ToTime(taistamp)
    val year = time.getYear
    val month = time.getMonthOfYear
    val date = time.getDayOfMonth
    val pattern = "%04d,%02d,%02d,%s,%s,%s.seq"
    pattern.format(year
                 , month
                 , date
                 , serviceInfo.serviceName
                 , serviceInfo.hostName
                 , taistamp)
  }

  def datePath(baseHdfsDir: HdfsDir, db: DateBucket)
  : HdfsDir = {
    val pattern = "%04d/%02d-%02d"
    new HdfsDir(baseHdfsDir, pattern.format(db.year, db.month, db.day))
  }

  def dateBucket(base: DateTime, daysBefore : Int = 0) : DateBucket = {
    val date = base.minusDays(daysBefore)
    DateBucket(date.getYear, date.getMonthOfYear, date.getDayOfMonth)
  }

  def dateStream(base: DateTime, startingDaysBefore: Int = 0)
  : Stream[DateBucket] = {

    def stream(base: DateTime, b: Int) : Stream[DateBucket] =
      dateBucket(base, b) #:: stream(base, b + 1)

    stream(base, startingDaysBefore)
  }

  private val hdfsFileMatcher =
    "([0-9]{4}),([0-9]{2}),([0-9]{2}),(.*),(.*),(.*).seq".r

  def getPlacedFileInfo(hdfsFile: HdfsFile)
  : Either[BarnError, PlacedFileInfo]
  = validate(
    hdfsFile.getName match {
      case hdfsFileMatcher(year, month, day, service, host, taistamp) =>
        Right(PlacedFileInfo(DateBucket(year.toInt, month.toInt, day.toInt)
                                , host, service, taistamp))
      case _ => Left(InvalidNameFormat("Invalid HdfsFile name format " + hdfsFile))
    }, "Invalid HdfsFile name format " + hdfsFile)

  def targetDirs(base: DateTime, baseHdfsDir: HdfsDir, lowerBound: DateTime)
  : Stream[HdfsDir] = {
    (dateBucket(base, 0) #::
     dateStream(base, 1).takeWhile(_ isAfter lowerBound.minusDays(1).toDateMidnight))
    .map(datePath(baseHdfsDir, _))
  }

  def targetTempDir(baseHdfsDir: HdfsDir)
  : HdfsDir
  = new HdfsDir(baseHdfsDir, "tmp/")

  implicit object PlacedFileOrderingByTaistamp extends Ordering[PlacedFileInfo] {
    def compare(o1: PlacedFileInfo, o2: PlacedFileInfo) : Int
    = o1.taistamp compare o2.taistamp
  }

  def sorted(list: List[PlacedFileInfo], service: LocalServiceInfo)
  : List[PlacedFileInfo] = list.sorted

  def filter(list: List[PlacedFileInfo], service: LocalServiceInfo)
  : List[PlacedFileInfo]
  = list.filter(matches(_, service))

  def matches(placedFile: PlacedFileInfo, serviceInfo: LocalServiceInfo)
  : Boolean
  = placedFile.service == serviceInfo.serviceName &&
    placedFile.host    == serviceInfo.hostName
}


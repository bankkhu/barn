package barn.placement

import org.joda.time._
import scala.math.Ordering
import barn._
import TimeUtils._

trait LocalPlacementStrategy
  extends Logging
  with SvlogdFile
  with LocalFS {

  val delim = '@'

  def decodeServiceInfo(serviceDir: Dir)
  : Either[BarnError, LocalServiceInfo]
  = serviceDir.getName.split(delim) match {
    case Array(service, category, host) =>
      Right(LocalServiceInfo(service, host))
    case _ => Left(InvalidNameFormat(s"Failed to extract service info for $serviceDir"))
  }

  def cleanupLocal(dir: Dir,
                   cleanupLimit: DateTime,
                   minMB: Int,
                   exclude: List[String] = List empty)
  : Either[BarnError, Unit]
  = for {
      localFiles <- listSortedLocalFiles(dir, exclude).right
      deletion   <- validate(Right({

        //Sum of the size of all files subject to deletion
        val sumSize = localFiles.foldLeft(0L) { (sum, f) => sum + f.length}

        localFiles.dropRight(1)
                  .foldLeft((0, sumSize)) {
          case ((deletedSoFar, curSize), file) =>
            val ts = Tai64.convertTai64ToTime(svlogdFileNameToTaiString(file.getName))

            if(ts.isBefore(cleanupLimit) && curSize > minMB*1024*1024) {
                val fileLength = file.length
                file.delete
                (deletedSoFar + 1) -> (curSize - fileLength)
            } else deletedSoFar -> curSize
        }
      }), "Deletion of retained files failed.").right
    } yield tap(deletion) { case(k, v) =>
      info(s"$k retained files deleted and ${v / 1024}KB remained on $dir")
  }
}


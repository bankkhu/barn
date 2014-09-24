package barn

import org.joda.time._

object SvlogdFile extends SvlogdFile

trait SvlogdFile {
  def svlogdFileTimestamp(svlogdFile: File)
  : Either[BarnError, DateTime] =
    validate(Right(Tai64.convertTai64ToTime(svlogdFileNameToTaiString(svlogdFile.getName))),
    "Couldn't extract timestamp from svlogd file")

  def svlogdFileNameToTaiString(svlogdFileName: String)
  : String = svlogdFileName.drop(1).dropRight(2)
}



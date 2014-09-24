package barn

object LocalFS extends LocalFS

trait LocalFS {


  def listSubdirectories(dir: Dir)
  : Either[BarnError, List[Dir]]
  = validate(Right(dir.listFiles.toList.filter(x => x.isDirectory)),
    "Can't get list of local services directories on: " + dir)

  def sumFileSizes(files: List[File]) : Long = {
    files.map(_.length).sum
  }

  def listSortedLocalFiles(dir : Dir, exclude: List[String] = List.empty)
  : Either[BarnError, List[File]] = validate (
    Right(dir.listFiles
       .toList
       .filterNot(_.isDirectory)
       .filterNot(x => exclude.foldLeft(false) {
          (res, pattern) => res || x.getName.matches(pattern) })
       .sorted)
  , "Can't list local files on: " + dir)

}

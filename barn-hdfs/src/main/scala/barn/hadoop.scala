package barn

object Hadoop extends Hadoop

trait Hadoop extends Logging {

  import org.apache.hadoop.util.GenericOptionsParser
  import org.apache.hadoop.fs.{Path, FileSystem => HdfsFileSystem}
  import org.apache.hadoop.conf.Configuration
  import scala.util.control.Exception.catching
  import java.io.FileNotFoundException

  type LazyHdfsFileSystem = LazyWrapper[HdfsFileSystem]

  type RemainingArgs = Array[String]

  def parseHadoopConf(args: Array[String]) : (RemainingArgs, Configuration) = {
    //Parse hadoop params and leave off extra ones
    val hadoopOptionParser = new GenericOptionsParser(new Configuration, args)
    val hadoopConf = hadoopOptionParser.getConfiguration()
    val remainingArgs = hadoopOptionParser.getRemainingArgs()
    (remainingArgs, hadoopConf)
  }

  def ensureHdfsDir(fs: HdfsFileSystem, hdfsDir: HdfsDir)
  : Either[BarnError, HdfsDir] = validate(
    Right(tap(hdfsDir)(createPath(fs, _)))
  , "Can't ensure/create dirs on hdfs:"  + hdfsDir)

  def pathExists(fs: HdfsFileSystem, hdfsDir: HdfsDir)
  : Either[BarnError, Boolean] = validate(
    Right(fs.exists(hdfsDir))
  , "Can't check existence of the path:" + hdfsDir)

  def createPath(fs: HdfsFileSystem, path: Path) : Boolean = fs.mkdirs(path)

  def createFileSystem(conf: Configuration)
  : Either[BarnError, HdfsFileSystem]
  = Right(HdfsFileSystem.get(conf))

  def createLazyFileSystem(conf: Configuration)
  : Either[BarnError, LazyHdfsFileSystem]
  = Right(new LazyWrapper(HdfsFileSystem.get(conf)))

  def listHdfsFiles(fs: HdfsFileSystem, hdfsDir: HdfsDir)
  : Either[BarnError, List[HdfsFile]]
  = validate(
      catching(classOf[FileNotFoundException])
      .either(fs.listStatus(hdfsDir)).fold(
        _ => Left(FileNotFound("Path " + hdfsDir + " doesn't exist to list")),
        _ match {
          case null => Left(FileNotFound("Path " + hdfsDir + " doesn't exist to list"))
          case fileList => Right(fileList.toList.filterNot(_.isDir).map(_.getPath))
        })
   , "Can't get list of files on HDFS dir: " + hdfsDir)

  def randomName() : String = scala.util.Random.alphanumeric.take(40).mkString

  def atomicShipToHdfs(fs: HdfsFileSystem
                     , src: File
                     , dest: HdfsDir
                     , hdfsName: String
                     , temp: HdfsDir)
  : Either[BarnError, HdfsFile]
  = for {
      hdfsTempFile <- shipToHdfs(fs, src, new HdfsDir(temp, randomName)).right
      renamedFile  <- atomicRenameOnHdfs(fs, hdfsTempFile, dest , hdfsName).right
  } yield renamedFile

  def atomicRenameOnHdfs(fs: HdfsFileSystem
                       , src: HdfsFile
                       , dest: HdfsDir
                       , newName: String)
  : Either[BarnError, HdfsFile]
  = validate({
    val targetHdfsFile = new HdfsFile(dest, newName)
    info("Moving " + src + " to " + targetHdfsFile + " @ "  + fs.getUri)
    fs.rename(src, targetHdfsFile) match {
      case true => Right(targetHdfsFile)
      case false =>
        Left(RenameFailed("Rename " + src + " to " + targetHdfsFile + " failed."))
    }}, "Rename failed due to IO error")

  def shipToHdfs(fs: HdfsFileSystem, localFile: File, targetFile: HdfsFile)
  : Either[BarnError, HdfsFile] = validate ({
    info("Shipping " + localFile + " to " + targetFile + " @ " + fs.getUri)
    fs.copyFromLocalFile(true, true, new HdfsFile(localFile.getPath), targetFile)
    Right(targetFile)
  }, "Can't ship to hdfs from " + localFile + " to " + targetFile )

}


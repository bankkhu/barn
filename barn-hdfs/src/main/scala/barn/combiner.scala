package barn

import org.apache.hadoop.fs.{FileSystem}
import org.apache.hadoop.io.SequenceFile
import org.apache.hadoop.io.SequenceFile.CompressionType
import org.apache.commons.io.IOUtils
import java.io.{FileInputStream, FileOutputStream}
import java.nio.charset.{Charset, CharsetDecoder, CodingErrorAction}
import scala.io.Source
import org.apache.hadoop.io.{BytesWritable => BW, LongWritable => LW, SequenceFile}
import org.apache.hadoop.conf.Configuration
import sun.misc.BASE64Decoder
import org.apache.commons.lang.RandomStringUtils

object FileCombiner extends FileCombiner

trait FileCombiner extends Logging {

  val utf8 = Charset.forName("UTF-8")

  //Combines files into a single file
  def combineFiles(localFiles: List[File], outputFile: File) = {
    val inputStreams = localFiles.map(x => new FileInputStream(x))
    val outputStream = new FileOutputStream(outputFile)

    inputStreams.foreach(is => IOUtils.copy(is, outputStream))

    inputStreams.foreach (_.close)
    outputStream.close
  }

  def getUTF8Decoder : CharsetDecoder = {
    val decoder = utf8.newDecoder()
    decoder.onMalformedInput(CodingErrorAction.REPLACE)
    decoder.replaceWith("?")
    decoder
  }

  //Combines local files into a local combined sequence file
  def combineIntoSeqFile(localFiles: List[File]
                       , outputFile: File) = {

    import java.io.{BufferedReader, InputStreamReader, FileInputStream}

    val conf = new Configuration
    val fs = FileSystem.getLocal(conf)
    val decoder = getUTF8Decoder

    val outputWriter = SequenceFile.createWriter( fs
                         , conf
                         , new HdfsDir(outputFile.getAbsolutePath)
                         , classOf[LW]
                         , classOf[BW]
                         , CompressionType.BLOCK
                         , compressionCodec
                       )

    val bufferSize = 20 * 1024 * 1024

    localFiles.foreach { file => 
      val bufferedSource = new BufferedReader(
                             new InputStreamReader(
                               new FileInputStream(file), decoder)
                           , bufferSize)

      try while(bufferedSource.ready)
            outputWriter.append( new LW(System.currentTimeMillis())
                               , new BW(bufferedSource.readLine.getBytes(utf8)))
      finally bufferedSource.close

    }

    outputWriter.close
    fs.close

  }

  def concatCandidates(candidates: List[File], targetDir: Dir)
  : Either[BarnError, File] = validate ({
    val combinedName = RandomStringUtils.randomAlphanumeric(20)
    val combinedLocalFile = new File(targetDir, combinedName)
    info(s"Combining ${candidates.size} (${candidates.head} and ... ) into $combinedLocalFile")
    combineIntoSeqFile(candidates, combinedLocalFile)
    Right(combinedLocalFile)
  } , s"Can't combine files into a sequence file. Candidates to combine: $candidates")


  import org.apache.hadoop.io.compress.CompressionCodec

  def compressionCodec : CompressionCodec = {
    import org.apache.hadoop.io.compress.DefaultCodec
    import org.apache.hadoop.io.compress.SnappyCodec

    try {
      SnappyCodec.checkNativeCodeLoaded()
      new SnappyCodec()
    } catch { case e : Throwable =>
      warn("Snappy codec isn't loaded. Using the default compression codec")
      new DefaultCodec()
    }
  }

}

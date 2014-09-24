package barn

import org.scalatest.FunSuite
import org.scalatest.BeforeAndAfter
import org.scalatest.prop.PropertyChecks
import org.scalatest.prop.Checkers
import org.apache.hadoop.conf.Configuration

class HadoopSuite
  extends FunSuite
  with Checkers
  with BeforeAndAfter
  with HadoopGenerators {

  import org.scalacheck.Arbitrary._
  import org.scalacheck.Prop._

  import Hadoop._
  import HadoopTestHelpers._

  import scala.util.Random.nextDouble

  val conf = tap(new Configuration()){_.set("fs.default.name"
                                          , "hdfs://localhost:9000")}
  val fs = Hadoop.createFileSystem(conf).right.get

  test("should correctly list files in a HDFS directory") {
    check(
      forAll(genHdfsDirWithFiles) { case (base, files) =>

        val correctResult = files.map(_.getName).toSet

        havingTemporaryDirStructure(fs, base, files) {
          listHdfsFiles(fs, base) fold (
            _ => false :| "IO exception occured when listing files.",
            listedFiles => (listedFiles.map(_.getName).toSet ==
                            correctResult) :|
                            "Doesn't match. Got: " + listedFiles
          )
        }
      }
    )
  }

}

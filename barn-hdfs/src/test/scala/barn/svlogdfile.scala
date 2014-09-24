package barn

import org.scalatest.FunSuite
import org.scalatest.prop.PropertyChecks
import org.scalatest.prop.Checkers
import org.scalacheck.Prop._
import SvlogdGenerators._

class SvlogdFileSuite
  extends FunSuite
  with Checkers
  with SvlogdFile {

  val tai64Length = 24

  test("Should strip svlogd file names' prefix properly") {
    check(
      forAll(genSvlogdFileName) { fileName =>
        val stripped = svlogdFileNameToTaiString(fileName)
        (stripped.size == tai64Length) :| "Invalid Tai64 timestamp"
      }
    )
  }

}

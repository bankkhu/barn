package barn

import org.scalatest.FunSuite
import org.scalatest.prop.PropertyChecks
import org.scalatest.prop.Checkers
import org.scalacheck.Prop._
import DateGenerators._

class Tai64Suite
  extends FunSuite
  with Checkers
  with SvlogdFile {

  import org.scalacheck.Arbitrary._
  import org.scalacheck.Prop._

  test("Tai64 timestamp conversion should be one-to-one") {
    check(
      forAll(genDate()) { date =>
        Tai64.convertTai64ToTime(Tai64.convertDateToTai64(date)) == date
      }
    )
  }

}


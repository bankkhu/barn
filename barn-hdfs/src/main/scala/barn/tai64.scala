package barn

object Tai64 extends Tai64

trait Tai64 {

  import org.joda.time._
  import org.apache.commons.lang.StringUtils.leftPad

  val baseDate = new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeZone.UTC);
  val taiBase = 4611686018427387904L

  def convertDateToTai64(time: DateTime) : String = {
    val extraSeconds = leapSecond(time)
    val leapAdjusted = time.plusSeconds(extraSeconds)
    val timeDelta = new Duration(baseDate, leapAdjusted)
    val seconds = timeDelta.getStandardSeconds
    val nanoInt = (timeDelta.getMillis - timeDelta.getStandardSeconds * 1000) * 1000000
    val taiInt = seconds + taiBase
    val intHex = java.lang.Long.toHexString(taiInt)
    val nanoHex = java.lang.Long.toHexString(nanoInt)
    leftPad(intHex, 16, '0') + leftPad(nanoHex, 8, '0')
  }

  def convertTai64ToTime(hex : String) : DateTime = {
    val taiInt = java.lang.Long.parseLong(hex.substring(0,16), 16)
    val nanoInt = java.lang.Long.parseLong(hex.substring(16, 24), 16)
    val seconds = taiInt - taiBase
    val timeDelta = new Duration(seconds * 1000 + nanoInt / 1000000)
    val final_ = baseDate.plus(timeDelta)
    val extraSeconds = leapSecond(final_)
    final_.plusSeconds(extraSeconds * -1)
  }

  def leapSecond(t: DateTime) : Int
  = conversionTable.reverse.find(_._1.isBefore(t)).map(_._2).getOrElse(0)

  val conversionTable = List[(DateTime, Int)](
                      (new DateTime(1972, 1,  1, 0 , 0), 10),
                      (new DateTime(1972, 7,  1, 0 , 0), 11),
                      (new DateTime(1973, 1,  1, 0 , 0), 12),
                      (new DateTime(1974, 1,  1, 0 , 0), 13),
                      (new DateTime(1975, 1,  1, 0 , 0), 14),
                      (new DateTime(1976, 1,  1, 0 , 0), 15),
                      (new DateTime(1977, 1,  1, 0 , 0), 16),
                      (new DateTime(1978, 1,  1, 0 , 0), 17),
                      (new DateTime(1979, 1,  1, 0 , 0), 18),
                      (new DateTime(1980, 1,  1, 0 , 0), 19),
                      (new DateTime(1981, 7,  1, 0 , 0), 20),
                      (new DateTime(1982, 7,  1, 0 , 0), 21),
                      (new DateTime(1983, 7,  1, 0 , 0), 22),
                      (new DateTime(1985, 7,  1, 0 , 0), 23),
                      (new DateTime(1988, 1,  1, 0 , 0), 24),
                      (new DateTime(1990, 1,  1, 0 , 0), 25),
                      (new DateTime(1991, 1,  1, 0 , 0), 26),
                      (new DateTime(1992, 7,  1, 0 , 0), 27),
                      (new DateTime(1993, 7,  1, 0 , 0), 28),
                      (new DateTime(1994, 7,  1, 0 , 0), 29),
                      (new DateTime(1996, 1,  1, 0 , 0), 30),
                      (new DateTime(1997, 7,  1, 0 , 0), 31),
                      (new DateTime(1999, 1,  1, 0 , 0), 32),
                      (new DateTime(2006, 1,  1, 0 , 0), 33),
                      (new DateTime(2009, 1,  1, 0 , 0), 34),
                      (new DateTime(2012, 7,  1, 0 , 0), 35),
                      (new DateTime(2015, 7,  1, 0 , 0), 36))


}


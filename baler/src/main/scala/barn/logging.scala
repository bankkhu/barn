package barn

import org.joda.time.format.DateTimeFormat
import org.joda.time.DateTime

trait Logging {

  val formatter = DateTimeFormat.forPattern("YYYY-MM-dd_HH:mm:ss.SSSSS")

  def now : String = formatter.print(DateTime.now)

  import java.lang.Thread.{currentThread => thread}

  def info(s: String) = println("INFO [" + thread.getName() + "] " + s)
  def warn(s: String) = println("WARN [" + thread.getName() + "] "+ s)
  def error(s: String) = println("ERROR [" + thread.getName() + "] "+ s)

  def logBarnError(context: String)(s: BarnError)
  = error("[" + context + "] " + s.toString)

}


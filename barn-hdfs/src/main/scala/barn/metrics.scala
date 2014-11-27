package barn

object Metrics {

  import scala.util.control.Exception._

  import javax.servlet.ServletException
  import org.eclipse.jetty.server.Server
  import org.eclipse.jetty.servlet.{ ServletContextHandler, ServletHolder }
  import io.prometheus.client.Prometheus
  import io.prometheus.client.utility.servlet.MetricsServlet

  import barn.placement.LocalServiceInfo


  def runWebServer[A](port: Int)(act: => A) : A = {
    val server = new Server(port)
    val ctx = new ServletContextHandler
    ctx.setContextPath("/")
    server.setHandler(ctx)
    ctx.addServlet(new ServletHolder(new MetricsServlet()), "/")
    server.start
    ultimately { server.stop } {
      Prometheus.defaultInitialize
      act
    }
  }

  object SyncMetrics {

    import io.prometheus.client.{ Prometheus, Register }
    import io.prometheus.client.metrics.{ Counter, Gauge }
    import org.joda.time.DateTime

    val BARN         = "barn"
    val BARN_HDFS    = "hdfs"
    val SERVICE_NAME = "service_name"
    val HOST_NAME    = "service_host"
    val RESULT       = "result"

    sealed trait Result
    case object Success extends Result
    case object Noop    extends Result
    case object Failed  extends Result

    def toResult[A](value: Either[BarnError,A]) : Result = value match {
      case Left(_:BarnFatalError) => Failed
      case Left(_)                => Noop
      case Right(_)               => Success
    }

    @Register
    val receivedDataGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("received_data")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("Number of bytes ready to be sync'ed.")
           .build

    @Register
    val maxFileAgeGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("ready_files_max_age")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("Maximum age, in ms, of local " +
                          "ready files according to their svlogdtimestamps.")
           .build

    @Register
    val lastShipGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("time_since_last_ship")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("Age, in ms, of the last file shipped " +
                          "according to svlogdtimestamps in file names.")
           .build

    @Register
    val shippedDataGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("shipped_data")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("The number of bytes last shipped.")
           .build

    @Register
    val shipCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("ship_count")
             .labelNames(SERVICE_NAME, HOST_NAME)
             .documentation("A count of the number files created on HDFS.")
             .build

    def setReceived( serviceInfo: LocalServiceInfo
                   , bytes      : Long
                   ) : Unit = {
      receivedDataGauge.newPartial
                       .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                       .labelPair(HOST_NAME, serviceInfo.hostName)
                       .apply
                       .set(bytes)
    }

    def setMinFileDate( serviceInfo: LocalServiceInfo
                      , minFileDate: DateTime ) : Unit =
      maxFileAgeGauge.newPartial
                     .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                     .labelPair(HOST_NAME, serviceInfo.hostName)
                     .apply
                     .set(System.currentTimeMillis - minFileDate.getMillis)

    def setLastShipDate( serviceInfo: LocalServiceInfo
                       , shipDate   : DateTime ) : Unit =
      lastShipGauge.newPartial
                   .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                   .labelPair(HOST_NAME, serviceInfo.hostName)
                   .apply
                   .set(System.currentTimeMillis - shipDate.getMillis)

    def setShipped( serviceInfo: LocalServiceInfo
                  , bytes      : Long
                  ) : Unit = {
      shipCounter.newPartial
                 .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                 .labelPair(HOST_NAME, serviceInfo.hostName)
                 .apply
                 .increment
      shippedDataGauge.newPartial
                      .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                      .labelPair(HOST_NAME, serviceInfo.hostName)
                      .apply
                      .set(bytes)
    }
  }
}

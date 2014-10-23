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
    val serviceGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("services")
           .documentation("The current number of services being syc'ed.")
           .build

    @Register
    val loopCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("loop_count")
             .documentation("A counter of the total sync loops made.")
             .build

    @Register
    val loopDurationGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("loop_duration")
           .documentation("The duration of the last sync loop in ms.")
           .build


    @Register
    val syncCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("sync_count")
             .labelNames(RESULT)
             .documentation("A counter of the total directory syncs made.")
             .build

    @Register
    val syncDurationGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("sync_duration")
           .documentation("The duration of the last directory sync in ms.")
           .build

    @Register
    val readyFilesGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("ready_files")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("Number of files ready to be sync'ed.")
           .build

    @Register
    val readyDataGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("ready_data")
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
    val concatCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("concat_count")
             .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
             .documentation("A count of how many times files are concatenated together locally.")
             .build

    @Register
    val concatDurationGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("concat_duration")
           .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
           .documentation("The duration of the last local concatenation is ms.")
           .build

    @Register
    val concatFilesGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("concat_files")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("The number of files last concatenated locally to one file.")
           .build

    @Register
    val concatDataGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("concat_data")
           .labelNames(SERVICE_NAME, HOST_NAME)
           .documentation("The number of bytes last concatenated locally to one file.")
           .build

    @Register
    val shipCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("ship_count")
             .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
             .documentation("A count of the number files created on HDFS.")
             .build

    @Register
    val shipDurationGauge =
      Gauge.newBuilder
           .namespace(BARN)
           .subsystem(BARN_HDFS)
           .name("ship_duration")
           .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
           .documentation("A histogram of the duration of ships to HDFS.")
           .build

    def setServiceCount(number: Long) : Unit = {
      serviceGauge.newPartial
                  .apply
                  .set(number)
    }

    def monitorLoop[A](act: => Unit) : Unit = {
      val start = System.currentTimeMillis
      val res   = act
      val time  = System.currentTimeMillis - start

      loopCounter.newPartial
                 .apply
                 .increment

      loopDurationGauge.newPartial
                       .apply
                       .set(time)
    }

    def monitorSync[A](act: => Either[BarnError,A]): Either[BarnError,A] =
      time( syncCounter
          , syncDurationGauge
          , None
          , act
          )

    def setReady( serviceInfo: LocalServiceInfo
                , files      : Long
                , bytes      : Long
                ) : Unit = {
      readyFilesGauge.newPartial
                     .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                     .labelPair(HOST_NAME, serviceInfo.hostName)
                     .apply
                     .set(files)
      readyDataGauge.newPartial
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

    def monitorConcat[A](serviceInfo: LocalServiceInfo)
                        (act: => Either[BarnError,A])
                        : Either[BarnError,A] =
      time( concatCounter
          , concatDurationGauge
          , Some(serviceInfo)
          , act)

    def setConcat( serviceInfo: LocalServiceInfo
                 , files      : Long
                 , bytes      : Long
                 ) : Unit = {
      concatFilesGauge.newPartial
                      .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                      .labelPair(HOST_NAME, serviceInfo.hostName)
                      .apply
                      .set(files)
      concatDataGauge.newPartial
                     .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                     .labelPair(HOST_NAME, serviceInfo.hostName)
                     .apply
                     .set(bytes)
    }

    def monitorShip[A](serviceInfo: LocalServiceInfo)
                      (act: => Either[BarnError,A])
                      : Either[BarnError,A] =
      time( shipCounter
          , shipDurationGauge
          , Some(serviceInfo)
          , act)

    private
    def time[A]( counter       : Counter
               , gauge         : Gauge
               , optServiceInfo: Option[LocalServiceInfo]
               , act: => Either[BarnError,A]): Either[BarnError,A] = {
      val start = System.currentTimeMillis
      val res   = act
      val time  = System.currentTimeMillis - start

      val (counterPartial, gaugePartial) = optServiceInfo match {
        case None       => (counter.newPartial, gauge.newPartial)
        case Some(info) =>
          ( counter.newPartial
                   .labelPair(SERVICE_NAME, info.serviceName)
                   .labelPair(HOST_NAME, info.hostName)
          , gauge.newPartial
                 .labelPair(SERVICE_NAME, info.serviceName)
                 .labelPair(HOST_NAME, info.hostName)
          )
      }

      counterPartial.labelPair(RESULT, toResult(res).toString)
                    .apply
                    .increment
      gaugePartial.labelPair(RESULT, toResult(res).toString)
                  .apply
                  .set(time)
      res
    }
  }
}

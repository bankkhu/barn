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
    import io.prometheus.client.metrics.{ Counter, Gauge, Summary }
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
    val syncCounter =
      Counter.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("sync_loop_count")
             .labelNames(RESULT)
             .documentation("A counter of the total sync loops made.")
             .build

    @Register
    val syncDurationSummary =
      Summary.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("sync_loop_duration")
             .documentation("A histogram of sync loop durations in ms.")
             .targetQuantile(0.01, 0.05)
             .targetQuantile(0.05, 0.05)
             .targetQuantile(0.5, 0.05)
             .targetQuantile(0.9, 0.01)
             .targetQuantile(0.99, 0.001)
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
    val concatDurationSummary =
      Summary.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("concat_duration")
             .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
             .documentation("A histogram of durations for local concatenations.")
             .targetQuantile(0.01, 0.05)
             .targetQuantile(0.05, 0.05)
             .targetQuantile(0.5, 0.05)
             .targetQuantile(0.9, 0.01)
             .targetQuantile(0.99, 0.001)
             .build

    @Register
    val concatFilesSummary =
      Summary.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("concat_files")
             .labelNames(SERVICE_NAME, HOST_NAME)
             .documentation("A histogram of the number of files concatenated locally to one file.")
             .targetQuantile(0.01, 0.05)
             .targetQuantile(0.05, 0.05)
             .targetQuantile(0.5, 0.05)
             .targetQuantile(0.9, 0.01)
             .targetQuantile(0.99, 0.001)
             .build

    @Register
    val concatDataSummary =
      Summary.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("concat_data")
             .labelNames(SERVICE_NAME, HOST_NAME)
             .documentation("A histogram of the number of bytes concatenated locally to one file.")
             .targetQuantile(0.01, 0.05)
             .targetQuantile(0.05, 0.05)
             .targetQuantile(0.5, 0.05)
             .targetQuantile(0.9, 0.01)
             .targetQuantile(0.99, 0.001)
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
    val shipDurationSummary =
      Summary.newBuilder
             .namespace(BARN)
             .subsystem(BARN_HDFS)
             .name("ship_duration")
             .labelNames(SERVICE_NAME, HOST_NAME, RESULT)
             .documentation("A histogram of the duration of ships to HDFS.")
             .targetQuantile(0.01, 0.05)
             .targetQuantile(0.05, 0.05)
             .targetQuantile(0.5, 0.05)
             .targetQuantile(0.9, 0.01)
             .targetQuantile(0.99, 0.001)
             .build

    def setServiceCount(number: Long) : Unit = {
      serviceGauge.newPartial
                  .apply
                  .set(number)
    }

    def monitorSync[A](act: => Either[BarnError,A]): Either[BarnError,A] =
      time( syncCounter
          , syncDurationSummary
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
          , concatDurationSummary
          , Some(serviceInfo)
          , act)

    def setConcat( serviceInfo: LocalServiceInfo
                 , files      : Long
                 , bytes      : Long
                 ) : Unit = {
      concatFilesSummary.newPartial
                        .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                        .labelPair(HOST_NAME, serviceInfo.hostName)
                        .apply
                        .observe(files)
      concatDataSummary.newPartial
                       .labelPair(SERVICE_NAME, serviceInfo.serviceName)
                       .labelPair(HOST_NAME, serviceInfo.hostName)
                       .apply
                       .observe(bytes)
    }

    def monitorShip[A](serviceInfo: LocalServiceInfo)
                      (act: => Either[BarnError,A])
                      : Either[BarnError,A] =
      time( shipCounter
          , shipDurationSummary
          , Some(serviceInfo)
          , act)

    private
    def time[A]( counter       : Counter
               , summary       : Summary
               , optServiceInfo: Option[LocalServiceInfo]
               , act: => Either[BarnError,A]): Either[BarnError,A] = {
      val start = System.currentTimeMillis
      val res   = act
      val time  = System.currentTimeMillis - start

      val (counterPartial, summaryPartial) = optServiceInfo match {
        case None       => (counter.newPartial, summary.newPartial)
        case Some(info) =>
          ( counter.newPartial
                   .labelPair(SERVICE_NAME, info.serviceName)
                   .labelPair(HOST_NAME, info.hostName)
          , summary.newPartial
                   .labelPair(SERVICE_NAME, info.serviceName)
                   .labelPair(HOST_NAME, info.hostName)
          )
      }

      counterPartial.labelPair(RESULT, toResult(res).toString)
                    .apply
                    .increment
      summaryPartial.labelPair(RESULT, toResult(res).toString)
                    .apply
                    .observe(time)
      res
    }
  }
}

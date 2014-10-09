package barn

object Metrics {

  import scala.util.control.Exception._

  import javax.servlet.ServletException
  import org.eclipse.jetty.server.Server
  import org.eclipse.jetty.servlet.{ ServletContextHandler, ServletHolder }
  import io.prometheus.client.Prometheus
  import io.prometheus.client.utility.servlet.MetricsServlet


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

  object SpoolMetrics {

    import io.prometheus.client.{ Prometheus, Register }
    import io.prometheus.client.metrics.Gauge

    val SERVICE_NAME = "service_name"
    val STAGE        = "stage"

    @Register
    val spoolGauge =
      Gauge.newBuilder
           .namespace("barn")
           .subsystem("barn-hdfs")
           .name("spool")
           .labelNames(SERVICE_NAME, STAGE)
           .documentation("Amount of data spooled at different stages")
           .build

    def record(serviceName: String, stage: String, amount: Long) : Unit =
      spoolGauge.newPartial
                .labelPair(SERVICE_NAME, serviceName)
                .labelPair(STAGE, stage)
                .apply
                .set(amount)
  }
}

package barn

import org.eclipse.jetty.server.Server
import org.eclipse.jetty.servlet.{ ServletContextHandler, ServletHolder }
import io.prometheus.client.Prometheus
import io.prometheus.client.utility.servlet.MetricsServlet
import io.prometheus.client.metrics.Gauge
import eventmonitor.validate.Validation.ValidationStatus

object PrometheusTelemetry {
  val namespace = "barn"
  val subsystem = "barn-hdfs"

  val spoolGauge =
    Gauge.newBuilder
           .namespace(namespace)
           .subsystem(subspace)
           .name("spool")
           .labelNames("service_name", "stage")
           .documentation("Amount of data spooled at different stages")
           .build

  def recordSpool(serviceName: String, stage: String, amount: Int) =
    spoolGauge.newPartial
               .labelPair("service_name", serviceName)
               .labelPair("stage", stage)
               .apply
               .set(amount)

  def start(port: Int) = {
    val server = new Server(port)
    val ctx = new ServletContextHandler
    ctx.setContextPath("/")
    server.setHandler(ctx)
    ctx.addServlet(new ServletHolder(new MetricsServlet()), "/")
    server.start
    Prometheus.defaultInitialize
  }
}

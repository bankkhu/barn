package barn

import java.util.concurrent.TimeUnit
import java.net.InetAddress
import com.codahale.metrics.ganglia.GangliaReporter
import com.codahale.metrics.JmxReporter
import com.codahale.metrics.MetricRegistry
import nl.grons.metrics.scala.InstrumentedBuilder
import info.ganglia.gmetric4j.gmetric.GMetric

object Instruments {
    val metricRegistry = new MetricRegistry()
}

trait Instruments extends InstrumentedBuilder {

  val metricRegistry = Instruments.metricRegistry

  private val currentlyShipping = metrics.counter("currentlyShipping")

  private val fatalError = metrics.meter("fatalError")

  private val combineTime = metrics.timer("combineTime")
  private val shipTime = metrics.timer("shipTime")
  private val cleanupTime = metrics.timer("cleanupTime")

  def reportShippingStarted = currentlyShipping += 1
  def reportShippingEnded = currentlyShipping -= 1

  def reportOngoingSync[A](f: => A): Unit = {
    reportShippingStarted
    try f finally reportShippingEnded
  }

  def reportFatalError = fatalError.mark
  def reportCombineTime[A](f: => A) = combineTime.time(f)
  def reportShipTime[A](f: => A) = shipTime.time(f)
  def reportCleanupTime[A](f: => A) = cleanupTime.time(f)

  case class GangliaOpts
    ( host:  String
    , port:     Int
    , interval: Int = 1
    )

  def enableJMX() : Unit = {
    val reporter = JmxReporter.forRegistry(metricRegistry)
                              .convertRatesTo(TimeUnit.MINUTES)
                              .convertDurationsTo(TimeUnit.MILLISECONDS)
                              .build()
    reporter.start()
  }

  def enableGanglia(app: String, opts: GangliaOpts): Unit = {
    val ganglia = new GMetric(opts.host, opts.port, GMetric.UDPAddressingMode.MULTICAST, 1)
    val reporter = GangliaReporter.forRegistry(metricRegistry)
                                  .convertRatesTo(TimeUnit.MINUTES)
                                  .convertDurationsTo(TimeUnit.MILLISECONDS)
                                  .build(ganglia)
     reporter.start(opts.interval, TimeUnit.MINUTES)
  }
}

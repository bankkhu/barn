package barn

import org.scalatest.FunSuite

class MetricsSuit extends FunSuite {
  test("Should normalize service names") {
    assert(Metrics.normalizeServiceName("some-service-da7bcb2-22691") == "some-service")
    assert(Metrics.normalizeServiceName("some-service-v2-da7bcb2-22691") == "some-service-v2")
  }
}
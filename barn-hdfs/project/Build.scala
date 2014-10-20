import sbt._
import Keys._

object BarnBuild extends Build {
    lazy val barn =
        Project("barn-hdfs", file("."))
            .configs(Slow)
            .settings(inConfig(Slow)(Defaults.testTasks) : _*)
            .settings(
              testOptions in Test := Seq(Tests.Filter(fastFilter))
            , testOptions in Slow := Seq(Tests.Filter(slowFilter))
            )

    def fastFilter(name: String): Boolean = {
        val s = name.toLowerCase
        // yeah, a bit lame. maybe a barn.slow package would be good
        !((s contains "hadoop") || (s contains "hdfs"))
    }

    def slowFilter(name: String): Boolean = !fastFilter(name)

    lazy val Slow = config("slow") extend (Test)
}

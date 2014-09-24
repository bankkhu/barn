package barn

import org.apache.hadoop.conf.{Configuration => HadoopConf}
import scopt.immutable.OptionParser

object ParamParser extends ParamParser

trait ParamParser
  extends Logging {

  val parser = new OptionParser[BarnConf]("$") {
    def options = Seq(
      opt("l", "local-log-dir", "<dir>", "directory containing the local logs.")
          {(v: String, c: BarnConf) => c.copy(localLogDir = new Dir(v))},
      opt("t", "local-temp-dir", "<dir>", "directory to write temp files.")
          {(v: String, c: BarnConf) => c.copy(localTempDir = new Dir(v))},
      opt("h", "hdfs-log-dir", "<dir>", "directory containing shipped logs.")
          {(v: String, c: BarnConf) => c.copy(hdfsLogDir = new HdfsDir(v))},
      opt("s", "hdfs", "<hdfs://host:port>", "hdfs endpoint")
          {(v: String, c: BarnConf) => c.copy(hdfsEndpoint =
            tap(new HadoopConf()){_.set("fs.default.name", v)})},
      booleanOpt("p", "run-parallel", "<bool>", "ship service logs in parallel. (default: false)")
          {(v: Boolean, c: BarnConf) => c.copy(runParallel = v)},
      intOpt("p", "deg-parallel", "<bool>", "degree of parallelsim (default: 2)")
          {(v: Int, c: BarnConf) => c.copy(degParallel = v)},
      intOpt("i", "ship-interval", "<int>", "How often to ship each service's logs. (sefault: 3600 s)")
          {(v: Int, c: BarnConf) => c.copy(shipInterval = v)},
      opt("ganglia-host", "Enables and specifies ganglia host")
          {(v: String, c: BarnConf) => c.copy(gangliaHost = v)},
      intOpt("ganglia-port", "Specifies ganglia port")
          {(v: Int, c: BarnConf) => c.copy(gangliaPort = v)}
    )
  }

  type LocalTempDir = Dir
  type LocalLogDir = Dir

  def loadConf(args: Array[String])
              (body: BarnConf => Unit)
  : Unit = {

    val barnConfDefault = BarnConf(null
                                 , null
                                 , null
                                 , null
                                 , false
                                 , 2
                                 , 3600
                                 , null
                                 , -1)

    parser.parse(args, barnConfDefault) map { config =>

      if(config.localLogDir == null || config.localTempDir == null ||
         config.hdfsLogDir == null || config.hdfsEndpoint == null ||
         config.gangliaHost == null || config.gangliaPort == -1)
        parser.showUsage
      else
        body(config)

    } getOrElse { false }
  }



}


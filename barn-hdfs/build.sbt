import AssemblyKeys._

name := "barn-hdfs"

version := "0.1.37-SNAPSHOT"

organization := "com.soundcloud"

scalaVersion := "2.10.4"

scalacOptions ++= Seq("-deprecation", "-unchecked", "-optimize")

javacOptions ++= Seq("-source", "1.6", "-target", "1.6")

resolvers := Seq(
   "SC Hosted"     at "http://maven.int.s-cloud.net/content/groups/hosted"
 , "SC Proxy"      at "http://maven.int.s-cloud.net/content/groups/proxy"
 , "Cloudera Repo" at "https://repository.cloudera.com/artifactory/cloudera-repos/"
 )

libraryDependencies := Seq(
  "joda-time"         %  "joda-time"    % "2.0"
, "org.joda"          %  "joda-convert" % "1.1"
//, "org.apache.hadoop" %  "hadoop-core"  % "0.20.2-cdh3u1"
, "org.apache.hadoop" % "hadoop-client" % "2.0.0-mr1-cdh4.3.0"
, "commons-io"        %  "commons-io"   % "2.4"
, "commons-lang"      %  "commons-lang" % "2.4"
, "org.scalatest"     % "scalatest_2.10"    % "2.1.0"  % "test,slow"
, "org.scalacheck"    % "scalacheck_2.10"   % "1.11.3" % "test,slow"
, "com.codahale.metrics" % "metrics-ganglia"  % "3.0.2"
, "com.codahale.metrics" % "metrics-servlet"  % "3.0.2"
, "nl.grons"          % "metrics-scala_2.10"  % "3.0.2"
, "javax.servlet"     % "javax.servlet-api"   % "3.0.1"
, "org.eclipse.jetty" % "jetty-server"        % "8.1.7.v20120910"
, "org.eclipse.jetty" % "jetty-servlet"       % "8.1.7.v20120910"
, "io.prometheus"     % "client"              % "0.0.3-pb2.4.1-SNAPSHOT"
, "io.prometheus.client.utility" % "servlet"  % "0.0.5-pb2.4.1-SNAPSHOT"
, "com.github.scopt"  % "scopt_2.10" % "2.1.0"
)


//
// settings
//

PackageDist.newSettings


//
//  test settings + options
//

testOptions in Test += Tests.Argument("-oDS")

testOptions in Slow += Tests.Argument("-oDS")

// will not work with sbt < 0.12
fork in Test := true

fork in Slow := true


//
// artifacts
//

crossPaths := false

publishArtifact in Test := false

publishArtifact in (Compile, packageDoc) := false

publishArtifact in (Compile, packageSrc) := false

addArtifact(Artifact("barn-hdfs", "zip", "zip"), packageDist)

publishTo <<= version { (v: String) =>
  val sc = "http://maven.int.s-cloud.net/content/repositories/"
  if (v.trim.endsWith("SNAPSHOT"))
    Some("snapshots" at sc + "snapshots")
  else
    Some("releases" at sc + "releases")
}

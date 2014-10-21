import sbt._
import Keys._

/** twitter's package-dist, the good parts */
object PackageDist extends Plugin {

  val packageDistDir =
    SettingKey[File]("package-dist-dir", "the directory to package dists into")

  val packageDist =
    TaskKey[File]("package-dist", "package a distribution for the current project")

  val packageDistName =
    SettingKey[String]("package-dist-name", "name of our distribution")

  val packageDistScriptsPath =
    SettingKey[Option[File]]("package-dist-scripts-path", "location of scripts (if any)")

  val packageDistScriptsOutputPath =
    SettingKey[Option[File]]("package-dist-scripts-output-path", "location of scripts output path")

  val packageDistZipName =
    TaskKey[String]("package-dist-zip-name", "name of packaged zip file")

  val packageDistZipPath =
    TaskKey[String]("package-dist-zip-path", "path of files inside the packaged zip file")

  val packageDistClean =
    TaskKey[Unit]("package-dist-clean", "clean distribution artifacts")

  val packageVars =
    TaskKey[Map[String, String]]("package-vars", "build a map of subtitutions for scripts")

  val packageDistCopyLibs =
    TaskKey[Set[File]]("package-dist-copy-libs", "copy scripts into the package dist folder")

  val packageDistCopyScripts =
    TaskKey[Set[File]]("package-dist-copy-scripts", "copy scripts into the package dist folder")

  val packageDistCopyJars =
    TaskKey[Set[File]]("package-dist-copy-jars", "copy exported files into the package dist folder")

  val packageDistCopy =
    TaskKey[Set[File]]("package-dist-copy", "copy all dist files into the package dist folder")


  // utility to copy a directory tree to a new one
  def copyTree(
    srcOpt: Option[File],
    destOpt: Option[File],
    selectedFiles: Option[Set[File]] = None
  ): Set[(File, File)] = {
    srcOpt.flatMap { src =>
      destOpt.map { dest =>
        val rebaser = Path.rebase(src, dest)
        selectedFiles.getOrElse {
          (PathFinder(src) ***).filter(!_.isDirectory).get
        }.flatMap { f =>
          rebaser(f) map { rebased =>
            (f, rebased)
          }
        }
      }
    }.getOrElse(Seq()).toSet
  }

  val newSettings = Seq(
    exportJars := true,

    // write a classpath entry to the manifest
    packageOptions <+= (dependencyClasspath in Compile, mainClass in Compile) map { (cp, main) =>
      val manifestClasspath = cp.files.map(f => "libs/" + f.getName).mkString(" ")
      // not sure why, but Main-Class needs to be set explicitly here.
      val attrs = Seq(("Class-Path", manifestClasspath)) ++ main.map { ("Main-Class", _) }
      Package.ManifestAttributes(attrs: _*)
    },
    packageDistName <<= (name, version) { (n, v) => n + "-" + v },
    packageDistDir <<= (baseDirectory, packageDistName) { (b, n) => b / "dist" / n },
    packageDistScriptsPath <<= (baseDirectory) { b => Some(b / "src" / "scripts") },
    packageDistScriptsOutputPath <<= (packageDistDir) { d => Some(d / "scripts") },
    packageDistZipPath <<= (packageDistName) map { name => name },
    packageDistZipName <<= (name,version) map { (n, v) => "%s-%s.zip".format(n,v) },

    packageVars <<= (
      dependencyClasspath in Runtime,
      exportedProducts in Compile,
      crossPaths,
      name,
      version,
      scalaVersion
    ) map { (rcp, exports, crossPaths, name, version, scalaVersion) =>
      val distClasspath = rcp.files.map("${DIST_HOME}/libs/" + _.getName) ++
        exports.files.map("${DIST_HOME}/" + _.getName)
      Map(
        "CLASSPATH" -> rcp.files.mkString(":"),
        "DIST_CLASSPATH" -> distClasspath.mkString(":"),
        "DIST_NAME" -> (if (crossPaths) (name + "_" + scalaVersion) else name),
        "VERSION" -> version
      )
    },

    packageDistCopyScripts <<= (
      packageVars,
      packageDistScriptsPath,
      packageDistScriptsOutputPath
    ) map { (vars, script, scriptOut) =>
      copyTree(script, scriptOut).map { case (source, destination) =>
        destination.getParentFile().mkdirs()
        FileFilter.filter(source, destination, vars)
        List("chmod", "+x", destination.absolutePath.toString) !!;
        destination
      }
    },

    packageDistCopyLibs <<= (
      dependencyClasspath in Runtime,
      exportedProducts in Compile,
      packageDistDir
    ) map { (cp, products, dest) =>
      val jarFiles = cp.files.filter(f => !products.files.contains(f))
      val jarDest = dest / "libs"
      jarDest.mkdirs()
      IO.copy(jarFiles.map { f => (f, jarDest / f.getName) })
    },

    packageDistCopyJars <<= (
      exportedProducts in Compile,
      packageDistDir
    ) map { (products, dest) =>
      IO.copy(products.files.map(p => (p, dest / p.getName)))
    },

    packageDistCopy <<= (
      packageDistCopyLibs,
      packageDistCopyScripts,
      packageDistCopyJars
    ) map { (libs, scripts, jars) =>
      libs ++ scripts ++ jars
    },

    // package all the things
    packageDist <<= (
      test in Test,
      baseDirectory,
      packageDistCopy,
      packageDistDir,
      packageDistName,
      packageDistZipPath,
      packageDistZipName,
      streams
    ) map { (_, base, files, dest, distName, zipPath, zipName, s) =>
      // build the zip
      s.log.info("Building %s from %d files.".format(zipName, files.size))
      val zipRebaser = Path.rebase(dest, zipPath)
      val zipFile = base / "dist" / zipName
      IO.zip(files.map(f => (f, zipRebaser(f).get)), zipFile)
      zipFile
    }
  )
}

import java.io.{ BufferedReader, FileReader, FileWriter, File }
object FileFilter {
  /**
   * Perform configure-style `@key@` substitution on a file as it's being copied.
   */
  def filter(source: File, destination: File, filters: Map[String, String]) {
    val in = new BufferedReader(new FileReader(source))
    val out = new FileWriter(destination)
    var line = in.readLine()
    while (line ne null) {
      filters.keys.foreach { token =>
        line = line.replace("@" + token + "@", filters(token))
      }
      out.write(line)
      out.write("\n")
      line = in.readLine()
    }
    in.close()
    out.close()
  }
}
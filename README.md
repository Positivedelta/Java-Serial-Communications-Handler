# Java Serial Communications Handler
#### Written using JNI (with C/C++) and supports Linux & Windows
The code included in this repository has been tested on,
- 32/64-bit ARM based Linux (Raspberry Pi 4)
- 64-bit Intel based Linux (x64 Ubuntu 22.04)
- Windows 7/10

It would not be difficult to extend this to support other operating systems or versions

Please note, this code was written for fun and probably contains bugs! However, I plan to evolve and support this code as I use it in my other projects

#### Prerequisites
- Java 8+
- Ant (use the most recent Linux and Windows versions)
- GCC 9+ (To build the Linux native shared library)
- Visual Studio 2022 (To build the Windows native DLL)

For testing and use on the Raspberry Pi, I installed Oracle's 32/64-bit ARM based JDK for Linux, download the following distribution jdk-8u271-linux-arm32-vfp-hflt.tar.gz or the latest Debian release for 64-bit systems

I used the Oracle JDK for performance reasons (supposedly the latest OpenJDK has caught up)

To install the Oracle JDK on a Raspberry Pi use the following procedure, this is assuming Buster Lite with no existing JDK / JRE (adapt these steps as necessary, e.g. to use a 64-bit JDK / JRE installer)
```
- sudo mkdir /usr/java
- cd /usr/java
- sudo tar xf ~/jdk-8u271-linux-arm32-vfp-hflt.tar.gz

- sudo update-alternatives --install /usr/bin/java java /usr/java/jdk1.8.0_271/bin/java 1000
- sudo update-alternatives --install /usr/bin/javac javac /usr/java/jdk1.8.0_271/bin/javac 1000
- sudo update-alternatives --install /usr/bin/javah javah /usr/java/jdk1.8.0_271/bin/javah 1000
- sudo update-alternatives --install /usr/bin/javap javap /usr/java/jdk1.8.0_271/bin/javap 1000
- sudo update-alternatives --install /usr/bin/jar jar /usr/java/jdk1.8.0_271/bin/jar 1000
```

Test using,
```
- java -version
- javac -version
```
To install Apache Ant on a Raspberry Pi use the following procedure, this is assuming Buster Lite and that the above JDK has been installed (you may need to adapt these steps)
```
- sudo mkdir /usr/ant
- cd /usr/ant
- sudo tar xf ~/apache-ant-1.10.9-bin.tar.gz
```

Then add the following to your `.bashrc` and use `source .bashrc` to refresh (or restart your bash terminal)
```
- export ANT_HOME=/usr/ant/apache-ant-1.10.9
- export JAVA_HOME=/usr/java/jdk1.8.0_271`
- export PATH=${PATH}:${ANT_HOME}/bin
```

Test using,
```
- ant -version
- javac -version
```
#### Notes
- The Linux Makefile builds the native shared library, the required JNI header file and hosts the Java sources
- Use Visual Studio to build the Windows DLL, note that you will need to keep the JNI header file synchronised with the Linux version if you update it
- Configure the Visual Studio build as `x64` in `Release` and then copy the resulting DLL to the windows directory (that's where the Ant build XML will expect it to be)
- Once the Linux and Windows native libraries have been built, use `ant jar` or `ant jar-log4j` in the root directory to create the final JAR file, note that the `ant jar` target does not include any of the supporting Log4J classes and associated configuration in the JAR as these are usually included separately in your main application
- Use `ant test` to build and run a simple test harness, note that the resulting JAR includes Log4J support
- Use `ant -p` to display all of the available build targets

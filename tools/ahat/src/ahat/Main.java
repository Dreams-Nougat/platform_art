/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package ahat;

import java.io.File;
import java.io.PrintStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

import com.android.tools.perflib.heap.io.HprofBuffer;
import com.android.tools.perflib.heap.io.MemoryMappedFileBuffer;
import com.android.tools.perflib.heap.*;

import com.sun.net.httpserver.*;

public class Main {

  public static void Help(PrintStream out) {
    out.println("java -jar ahat.jar [-p port] FILE");
    out.println("  Launch an http server for viewing "
        + "the given Android heap-dump FILE.");
    out.println("");
    out.println("Options:");
    out.println("  -p <port>");
    out.println("     Serve pages on the given port. Defaults to 7100.");
    out.println("");
  }

  public static void main(String[] args) throws Exception {
    int port = 7100;
    for (String arg : args) {
      if (arg.equals("--help")) {
        Help(System.out);
        return;
      }
    }

    File hprof = null;
    for (int i = 0; i < args.length; i++) {
      if ("-p".equals(args[i]) && i+1 < args.length) {
        i++;
        port = Integer.parseInt(args[i]);
      } else {
        if (hprof != null) {
          System.err.println("multiple input files.");
          Help(System.err);
          return;
        }
        hprof = new File(args[i]);
      }
    }

    if (hprof == null) {
      System.err.println("no input file.");
      Help(System.err);
      return;
    }

    System.out.println("Reading hprof file...");
    HprofBuffer buffer = new MemoryMappedFileBuffer(hprof);
    Snapshot snapshot = (new HprofParser(buffer)).parse();

    System.out.println("Computing Dominators...");
    snapshot.computeDominators();

    System.out.println("Processing snapshot for ahat...");
    AhatSnapshot ahat = new AhatSnapshot(snapshot);

    InetAddress loopback = InetAddress.getLoopbackAddress();
    InetSocketAddress addr = new InetSocketAddress(loopback, port);
    HttpServer server = HttpServer.create(addr, 0);
    server.createContext("/", new OverviewHandler(ahat, hprof));
    server.createContext("/roots", new RootsHandler(ahat));
    server.createContext("/object", new ObjectHandler(ahat));
    server.createContext("/objects", new ObjectsHandler(ahat));
    server.createContext("/site", new SiteHandler(ahat));
    server.createContext("/bitmap", new BitmapHandler(ahat));
    server.createContext("/help", new StaticHandler("help.html", "text/html"));
    server.createContext("/style.css", new StaticHandler("style.css", "text/css"));
    server.setExecutor(Executors.newFixedThreadPool(1));
    System.out.println("Server started on localhost:" + port);
    server.start();
  }
}


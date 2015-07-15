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

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

import com.google.common.io.ByteStreams;
import com.sun.net.httpserver.*;

// Handler that returns a static file included in ahat.jar.
class StaticHandler implements HttpHandler {
  private String mResourceName;
  private String mContentType;

  public StaticHandler(String resourceName, String contentType) {
    mResourceName = resourceName;
    mContentType = contentType;
  }

  public void handle(HttpExchange exchange) throws IOException {
    ClassLoader loader = StaticHandler.class.getClassLoader();
    InputStream is = loader.getResourceAsStream(mResourceName);
    if (is == null) {
      exchange.getResponseHeaders().add("Content-Type", "text/html");
      exchange.sendResponseHeaders(404, 0);
      PrintStream ps = new PrintStream(exchange.getResponseBody());
      ps.println("Resource not found.");
      ps.close();
    } else {
      exchange.getResponseHeaders().add("Content-Type", mContentType);
      exchange.sendResponseHeaders(200, 0);
      OutputStream os = exchange.getResponseBody();
      ByteStreams.copy(is, os);
      os.close();
    }
  }
}


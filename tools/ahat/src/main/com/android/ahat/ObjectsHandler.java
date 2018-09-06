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

package com.android.ahat;

import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Site;
import com.android.ahat.heapdump.Sort;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Predicate;

class ObjectsHandler implements AhatHandler {
  private static final String OBJECTS_ID = "objects";

  private AhatSnapshot mSnapshot;

  public ObjectsHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  /**
   * Get the list of instances that match the given site, class, and heap
   * filters. This method is public to facilitate testing.
   *
   * @param site the site to get instances from
   * @param className non-null name of the class to restrict instances to.
   * @param subclass if true, include instances of subclasses of the named class.
   * @param heapName name of the heap to restrict instances to. May be null to
   *                 allow instances on any heap.
   * @return list of matching instances
   */
  public static List<AhatInstance> getObjects(
      Site site, String className, boolean subclass, String heapName) {
    Predicate<AhatInstance> predicate = (x) -> {
      return (heapName == null || x.getHeap().getName().equals(heapName))
        && (subclass ? x.isInstanceOfClass(className) : className.equals(x.getClassName()));
    };

    List<AhatInstance> insts = new ArrayList<AhatInstance>();
    site.getObjects(predicate, x -> insts.add(x));
    return insts;
  }

  @Override
  public void handle(Doc doc, Query query) throws IOException {
    int id = query.getInt("id", 0);
    String className = query.get("class", "java.lang.Object");
    String heapName = query.get("heap", null);
    boolean subclass = (query.getInt("subclass", 0) != 0);
    Site site = mSnapshot.getSite(id);

    List<AhatInstance> insts = getObjects(site, className, subclass, heapName);
    Collections.sort(insts, Sort.defaultInstanceCompare(mSnapshot));

    doc.title("Instances");

    // Write a description of the current settings, with links to adjust the
    // settings, such as:
    //    Site:           ROOT -
    //    Class:          android.os.Binder
    //    Subclasses:     excluded (switch to included)
    //    Heap:           any (switch to app, image, zygote)
    //    Count:          17,424
    doc.descriptions();
    doc.description(DocString.text("Site"), Summarizer.summarize(site));
    doc.description(DocString.text("Class"), DocString.text(className));

    DocString subclassChoice = DocString.text(subclass ? "included" : "excluded");
    subclassChoice.append(" (switch to ");
    subclassChoice.appendLink(query.with("subclass", subclass ? 0 : 1),
      DocString.text(subclass ? "excluded" : "included"));
    subclassChoice.append(")");
    doc.description(DocString.text("Subclasses"), subclassChoice);

    DocString heapChoice = DocString.text(heapName == null ? "any" : heapName);
    heapChoice.append(" (switch to ");
    String comma = "";
    for (AhatHeap heap : mSnapshot.getHeaps()) {
      if (!heap.getName().equals(heapName)) {
        heapChoice.append(comma);
        heapChoice.appendLink(
            query.with("heap", heap.getName()),
            DocString.text(heap.getName()));
        comma = ", ";
      }
    }
    if (heapName != null) {
      heapChoice.append(comma);
      heapChoice.appendLink(
          query.with("heap", null),
          DocString.text("any"));
    }
    heapChoice.append(")");
    doc.description(DocString.text("Heap"), heapChoice);

    doc.description(DocString.text("Count"), DocString.format("%,14d", insts.size()));
    doc.end();
    doc.println(DocString.text(""));

    if (insts.isEmpty()) {
      doc.println(DocString.text("(none)"));
    } else {
      SizeTable.table(doc, mSnapshot.isDiffed(),
          new Column("Heap"),
          new Column("Object"));

      SubsetSelector<AhatInstance> selector = new SubsetSelector(query, OBJECTS_ID, insts);
      for (AhatInstance inst : selector.selected()) {
        AhatInstance base = inst.getBaseline();
        SizeTable.row(doc, inst.getSize(), base.getSize(),
            DocString.text(inst.getHeap().getName()),
            Summarizer.summarize(inst));
      }
      SizeTable.end(doc);
      selector.render(doc);
    }
  }
}


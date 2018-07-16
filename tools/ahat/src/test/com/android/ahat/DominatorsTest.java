/*
 * Copyright (C) 2017 The Android Open Source Project
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

import com.android.ahat.dominators.Dominators;
import com.android.ahat.dominators.DominatorsComputation;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.assertEquals;

public class DominatorsTest {

  private static class Graph implements Dominators.Graph<String> {
    private Map<String, Object> states = new HashMap<>();
    private Map<String, Collection<String>> depends = new HashMap<>();
    private Map<String, String> dominators = new HashMap<>();

    @Override
    public void setDominatorsComputationState(String node, Object state) {
      states.put(node, state);
    }

    @Override public Object getDominatorsComputationState(String node) {
      return states.get(node);
    }

    @Override
    public Collection<String> getReferencesForDominators(String node) {
      return depends.get(node);
    }

    @Override
    public void setDominator(String node, String dominator) {
      dominators.put(node, dominator);
    }

    /**
     * Define a node in the graph, including all its outgoing edges.
     */
    public void node(String src, String... dsts) {
      depends.put(src, Arrays.asList(dsts));
    }

    /**
     * Get the computed dominator for a given node.
     */
    public String dom(String node) {
      return dominators.get(node);
    }
  }

  @Test
  public void singleNode() {
    // --> n
    // Trivial case.
    Graph graph = new Graph();
    graph.node("n");
    new Dominators(graph).computeDominators("n");
  }

  @Test
  public void parentWithChild() {
    // --> parent --> child
    // The child node is dominated by the parent.
    Graph graph = new Graph();
    graph.node("parent", "child");
    graph.node("child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("child"));
  }

  @Test
  public void reachableTwoWays() {
    //            /-> right -->\
    // --> parent               child
    //            \-> left --->/
    // The child node can be reached either by right or by left.
    Graph graph = new Graph();
    graph.node("parent", "left", "right");
    graph.node("right", "child");
    graph.node("left", "child");
    graph.node("child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("left"));
    assertEquals("parent", graph.dom("right"));
    assertEquals("parent", graph.dom("child"));
  }

  @Test
  public void reachableDirectAndIndirect() {
    //            /-> right -->\
    // --> parent  -----------> child
    // The child node can be reached either by right or parent.
    Graph graph = new Graph();
    graph.node("parent", "right", "child");
    graph.node("right", "child");
    graph.node("child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("child"));
    assertEquals("parent", graph.dom("right"));
  }

  @Test
  public void subDominator() {
    // --> parent --> middle --> child
    // The child is dominated by an internal node.
    Graph graph = new Graph();
    graph.node("parent", "middle");
    graph.node("middle", "child");
    graph.node("child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("middle"));
    assertEquals("middle", graph.dom("child"));
  }

  @Test
  public void childSelfLoop() {
    // --> parent --> child -\
    //                  \<---/
    // The child points back to itself.
    Graph graph = new Graph();
    graph.node("parent", "child");
    graph.node("child", "child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("child"));
  }

  @Test
  public void singleEntryLoop() {
    // --> parent --> a --> b --> c -\
    //                 \<------------/
    // There is a loop in the graph, with only one way into the loop.
    Graph graph = new Graph();
    graph.node("parent", "a");
    graph.node("a", "b");
    graph.node("b", "c");
    graph.node("c", "a");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("a"));
    assertEquals("a", graph.dom("b"));
    assertEquals("b", graph.dom("c"));
  }

  @Test
  public void multiEntryLoop() {
    // --> parent --> right --> a --> b ----\
    //        \                  \<-- c <---/
    //         \--> left --->--------/
    // There is a loop in the graph, with two different ways to enter the
    // loop.
    Graph graph = new Graph();
    graph.node("parent", "left", "right");
    graph.node("left", "c");
    graph.node("right", "a");
    graph.node("a", "b");
    graph.node("b", "c");
    graph.node("c", "a");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("right"));
    assertEquals("parent", graph.dom("left"));
    assertEquals("parent", graph.dom("a"));
    assertEquals("parent", graph.dom("c"));
    assertEquals("a", graph.dom("b"));
  }

  @Test
  public void dominatorOverwrite() {
    //            /---------> right <--\
    // --> parent  --> child <--/      /
    //            \---> left ---------/
    // Test a strange case where we have had trouble in the past with a
    // dominator getting improperly overwritten. The relevant features of this
    // case are: 'child' is visited after 'right', 'child' is dominated by
    // 'parent', and 'parent' revisits 'right' after visiting 'child'.
    Graph graph = new Graph();
    graph.node("parent", "left", "child", "right");
    graph.node("right", "child");
    graph.node("left", "right");
    graph.node("child");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("left"));
    assertEquals("parent", graph.dom("child"));
    assertEquals("parent", graph.dom("right"));
  }

  @Test
  public void stackOverflow() {
    // --> a --> b --> ... --> N
    // Verify we don't smash the stack for deep chains.
    Graph graph = new Graph();
    String root = "end";
    graph.node(root);

    for (int i = 0; i < 10000; ++i) {
      String child = root;
      root = "n" + i;
      graph.node(root, child);
    }

    new Dominators(graph).computeDominators(root);
  }

  @Test
  public void hiddenRevisit() {
    //           /-> left ---->---------\
    // --> parent      \---> a --> b --> c
    //           \-> right -/
    // Test a case we have had trouble in the past.
    // When a's dominator is updated from left to parent, that should trigger
    // all reachable children's dominators to be updated too. In particular,
    // c's dominator should be updated, even though b's dominator is
    // unchanged.
    Graph graph = new Graph();
    graph.node("parent", "right", "left");
    graph.node("right", "a");
    graph.node("left", "a", "c");
    graph.node("a", "b");
    graph.node("b", "c");
    graph.node("c");
    new Dominators(graph).computeDominators("parent");

    assertEquals("parent", graph.dom("left"));
    assertEquals("parent", graph.dom("right"));
    assertEquals("parent", graph.dom("a"));
    assertEquals("parent", graph.dom("c"));
    assertEquals("a", graph.dom("b"));
  }

  @Test
  public void preUndominatedUpdate() {
    //       /--------->--------\
    //      /          /---->----\
    // --> p -> a --> b --> c --> d --> e
    //           \---------->----------/
    // Test a case we have had trouble in the past.
    // The candidate dominator for e is revised from d to a, then d is shown
    // to be reachable from p. Make sure that causes e's dominator to be
    // refined again from a to p. The extra nodes are there to ensure the
    // necessary scheduling to expose the bug we had.
    Graph graph = new Graph();
    graph.node("p", "d", "a");
    graph.node("a", "e", "b");
    graph.node("b", "d", "c");
    graph.node("c", "d");
    graph.node("d", "e");
    graph.node("e");
    new Dominators(graph).computeDominators("p");

    assertEquals("p", graph.dom("a"));
    assertEquals("a", graph.dom("b"));
    assertEquals("b", graph.dom("c"));
    assertEquals("p", graph.dom("d"));
    assertEquals("p", graph.dom("e"));
  }

  @Test
  public void twiceRevisit() {
    //       /---->---\
    //      /     /--> f -->-\
    // --> a --> b -->--x---> c --> d
    //            \----------->----/
    // A regression test for a bug where we failed to ever revisit a node more
    // than once. The node c is revisited a first time to bring its dominator
    // up to b. c needs to be revisited again after the dominator for f is
    // pulled up to a, and that revisit of c is necessary to ensure the
    // dominator for d is pulled up to a.
    Graph graph = new Graph();
    graph.node("a", "f", "b");
    graph.node("b", "f", "d", "x");
    graph.node("x", "c");
    graph.node("c", "d");
    graph.node("d");
    graph.node("f", "c");
    new Dominators(graph).computeDominators("a");

    assertEquals("a", graph.dom("b"));
    assertEquals("b", graph.dom("x"));
    assertEquals("a", graph.dom("c"));
    assertEquals("a", graph.dom("d"));
    assertEquals("a", graph.dom("f"));
  }

  // Test the old dominators API.
  private static class Node implements DominatorsComputation.Node {
    public String name;
    public List<Node> depends = new ArrayList<Node>();
    public Node dominator;
    private Object dominatorsComputationState;

    public Node(String name) {
      this.name = name;
    }

    public void computeDominators() {
      DominatorsComputation.computeDominators(this);
    }

    public String toString() {
      return name;
    }

    @Override
    public void setDominatorsComputationState(Object state) {
      dominatorsComputationState = state;
    }

    @Override
    public Object getDominatorsComputationState() {
      return dominatorsComputationState;
    }

    @Override
    public Collection<Node> getReferencesForDominators() {
      return depends;
    }

    @Override
    public void setDominator(DominatorsComputation.Node dominator) {
      this.dominator = (Node)dominator;
    }
  }

  @Test
  public void twiceRevisitOldApi() {
    //       /---->---\
    //      /     /--> f -->-\
    // --> a --> b -->--x---> c --> d
    //            \----------->----/
    // Run the twiceRevisit test using the user api version of computing
    // dominators.
    Node a = new Node("a");
    Node b = new Node("b");
    Node x = new Node("x");
    Node c = new Node("c");
    Node d = new Node("d");
    Node f = new Node("f");
    a.depends = Arrays.asList(f, b);
    b.depends = Arrays.asList(f, d, x);
    x.depends = Arrays.asList(c);
    c.depends = Arrays.asList(d);
    f.depends = Arrays.asList(c);

    a.computeDominators();
    assertEquals(a, b.dominator);
    assertEquals(b, x.dominator);
    assertEquals(a, c.dominator);
    assertEquals(a, d.dominator);
    assertEquals(a, f.dominator);
  }
}

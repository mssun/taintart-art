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

package com.android.ahat.dominators;

import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.Deque;
import java.util.Queue;

/**
 * Provides a static method for computing the immediate dominators of a
 * directed graph. It can be used with any directed graph data structure
 * that implements the {@link DominatorsComputation.Node} interface and has
 * some root node with no incoming edges.
 */
public class DominatorsComputation {
  private DominatorsComputation() {
  }

  /**
   * Interface for a directed graph to perform immediate dominators
   * computation on.
   * The dominators computation can be used with directed graph data
   * structures that implement this <code>Node</code> interface. To use the
   * dominators computation on your graph, you must make the following
   * functionality available to the dominators computation:
   * <ul>
   * <li>Efficiently mapping from node to associated internal dominators
   *     computation state using the
   *     {@link #setDominatorsComputationState setDominatorsComputationState} and
   *     {@link #getDominatorsComputationState getDominatorsComputationState} methods.
   * <li>Iterating over all outgoing edges of an node using the
   *     {@link #getReferencesForDominators getReferencesForDominators} method.
   * <li>Setting the computed dominator for a node using the
   *     {@link #setDominator setDominator} method.
   * </ul>
   */
  public interface Node {
    /**
     * Associates the given dominator state with this node. Subsequent calls to
     * {@link #getDominatorsComputationState getDominatorsComputationState} on
     * this node should return the state given here. At the conclusion of the
     * dominators computation, this method will be called for
     * each node with <code>state</code> set to null.
     *
     * @param state the dominator state to associate with this node
     */
    void setDominatorsComputationState(Object state);

    /**
     * Returns the dominator state most recently associated with this node
     * by a call to {@link #setDominatorsComputationState setDominatorsComputationState}.
     * If <code>setDominatorsComputationState</code> has not yet been called
     * on this node for this dominators computation, this method should return
     * null.
     *
     * @return the associated dominator state
     */
    Object getDominatorsComputationState();

    /**
     * Returns a collection of nodes referenced from this node, for the
     * purposes of computing dominators. This method will be called at most
     * once for each node reachable from the root node of the dominators
     * computation.
     *
     * @return an iterable collection of the nodes with an incoming edge from
     *         this node.
     */
    Iterable<? extends Node> getReferencesForDominators();

    /**
     * Sets the dominator for this node based on the results of the dominators
     * computation.
     *
     * @param dominator the computed immediate dominator of this node
     */
    void setDominator(Node dominator);
  }

  // NodeS is information associated with a particular node for the
  // purposes of computing dominators.
  // By convention we use the suffix 'S' to name instances of NodeS.
  private static class NodeS {
    // The node that this NodeS is associated with.
    public Node node;

    // Unique identifier for this node, in increasing order based on the order
    // this node was visited in a depth first search from the root. In
    // particular, given nodes A and B, if A.id > B.id, then A cannot be a
    // dominator of B.
    public long id;

    // The largest id of all nodes reachable from this node.
    // If foo.id > this.maxReachableId, then foo is not reachable from this
    // node.
    public long maxReachableId;

    // The set of ids of nodes that have references to this node.
    public IdSet inRefIds = new IdSet();

    // The current candidate dominator for this node.
    // The true immediate dominator of this node must have id <= domS.id.
    public NodeS domS;

    // The previous candidate dominator for this node.
    // Invariant:
    // * There are no nodes xS reachable from this node on a path of nodes
    //   with increasing ids (not counting xS.id) for which
    //   this.id > xS.domS.id > this.oldDomS.id.
    // This ensures that when all nodes xS satisfy xS.domS == xS.oldDomS, we
    // have found the true immediate dominator of each node.
    //
    // Note: We only use this field to tell if this node is scheduled to be
    // revisited. We could replace it with a boolean to save space, but it
    // probably doesn't save that much space and it's easier to explain the
    // algorithm if we can refer to this field.
    public NodeS oldDomS;

    // The set of nodes that this node is the candidate immediate dominator
    // of. More precisely, the set of nodes xS such that xS.domS == this.
    public NodeSet dominated = new NodeSet();

    // The set of nodes that this node is the old candidate immediate
    // dominator of that need to be revisited. Specifically, the set of nodes
    // xS such that:
    //   xS.oldDomS == this && xS.oldDomS != xS.domS.
    //
    // The empty set is represented as null instead of an empty NodeSet to
    // save memory.
    // Invariant:
    //   If revisit != null, this node is on the global list of nodes to be
    //   revisited.
    public NodeSet revisit = null;
  }

  // A collection of node ids.
  private static class IdSet {
    private int size = 0;
    private long[] ids = new long[4];

    // Adds an id to the set.
    public void add(long id) {
      if (size == ids.length) {
        ids = Arrays.copyOf(ids, size * 2);
      }
      ids[size++] = id;
    }

    // Returns the most recent id added to the set. Behavior is undefined if
    // the set is empty.
    public long last() {
      assert size != 0;
      return ids[size - 1];
    }

    // Returns true if the set contains an id in the range [low, high]
    // inclusive, false otherwise.
    public boolean hasIdInRange(long low, long high) {
      for (int i = 0; i < size; ++i) {
        if (low <= ids[i] && ids[i] <= high) {
          return true;
        }
      }
      return false;
    }
  }

  // An unordered set of nodes data structure supporting efficient iteration
  // over elements. The bulk of the time spent in the dominators algorithm is
  // iterating over these sets. Using an array to store the set provides
  // noticable performance improvements over ArrayList or a linked list.
  private static class NodeSet {
    public int size = 0;
    public NodeS[] nodes = new NodeS[4];

    public void add(NodeS nodeS) {
      if (size == nodes.length) {
        nodes = Arrays.copyOf(nodes, size * 2);
      }
      nodes[size++] = nodeS;
    }

    public void remove(NodeS nodeS) {
      for (int i = 0; i < size; ++i) {
        if (nodes[i] == nodeS) {
          remove(i);
          break;
        }
      }
    }

    public void remove(int index) {
      nodes[index] = nodes[--size];
      nodes[size] = null;
    }
  }

  // A reference from a source node to a destination node to be processed
  // during the initial depth-first traversal of nodes.
  //
  // Also used as a marker to indicate when the depth-first traversal has been
  // completed for a node. In that case, srcS is the node depth-first
  // traversal has been completed for, and dst will be set to null.
  private static class Link {
    public final NodeS srcS;
    public final Node dst;

    // Constructor for a reference from srcS to dst.
    public Link(NodeS srcS, Node dst) {
      this.srcS = srcS;
      this.dst = dst;
    }

    // Constructor for a marker indicating depth-first traversal has been
    // completed for srcS.
    public Link(NodeS srcS) {
      this.srcS = srcS;
      this.dst = null;
    }
  }

  /**
   * Computes the immediate dominators of all nodes reachable from the <code>root</code> node.
   * There must not be any incoming references to the <code>root</code> node.
   * <p>
   * The result of this function is to call the {@link Node#setDominator}
   * function on every node reachable from the root node.
   *
   * @param root the root node of the dominators computation
   * @see Node
   */
  public static void computeDominators(Node root) {
    long id = 0;

    // The set of nodes xS such that xS.revisit != null.
    // Use a Queue instead of a Set because performance will be better. We
    // avoid adding nodes already on the queue by checking
    // xS == null before adding the node to the queue.
    Queue<NodeS> revisit = new ArrayDeque<NodeS>();

    // Set up the root node specially.
    NodeS rootS = new NodeS();
    rootS.node = root;
    rootS.id = id++;
    root.setDominatorsComputationState(rootS);

    Deque<Link> dfs = new ArrayDeque<Link>();
    dfs.push(new Link(rootS));
    for (Node child : root.getReferencesForDominators()) {
      dfs.push(new Link(rootS, child));
    }

    // 1. Do a depth first search of the nodes, label them with ids and come
    // up with initial candidate dominators for them.
    while (!dfs.isEmpty()) {
      Link link = dfs.pop();

      if (link.dst == null) {
        // This is the marker link indicating we have now visited all
        // nodes reachable from link.srcS.
        link.srcS.maxReachableId = id - 1;
      } else {
        NodeS dstS = (NodeS)link.dst.getDominatorsComputationState();
        if (dstS == null) {
          // We are seeing the destination node for the first time.
          // The candidate dominator is the source node.
          dstS = new NodeS();
          link.dst.setDominatorsComputationState(dstS);

          dstS.node = link.dst;
          dstS.id = id++;
          dstS.inRefIds.add(link.srcS.id);
          dstS.domS = link.srcS;
          dstS.domS.dominated.add(dstS);
          dstS.oldDomS = link.srcS;

          dfs.push(new Link(dstS));
          for (Node child : link.dst.getReferencesForDominators()) {
            dfs.push(new Link(dstS, child));
          }
        } else {
          // We have seen the destination node before. Update the state based
          // on the new potential dominator.
          long seenid = dstS.inRefIds.last();
          dstS.inRefIds.add(link.srcS.id);

          // Go up the dominator chain until we reach a node we haven't already
          // seen with a path to dstS.
          NodeS xS = link.srcS;
          while (xS.id > seenid) {
            xS = xS.domS;
          }

          // The new dominator for dstS must have an id less than the node we
          // just reached. Pull the dominator for dstS up its dominator
          // chain until we find a suitable new dominator for dstS.
          long domid = xS.id;
          if (dstS.domS.id > domid) {
            // Mark the node as needing to be revisited.
            if (dstS.domS == dstS.oldDomS) {
              if (dstS.oldDomS.revisit == null) {
                dstS.oldDomS.revisit = new NodeSet();
                revisit.add(dstS.oldDomS);
              }
              dstS.oldDomS.revisit.add(dstS);
            }

            // Update the node's candidate dominator.
            dstS.domS.dominated.remove(dstS);
            do {
              dstS.domS = dstS.domS.domS;
            } while (dstS.domS.id > domid);
            dstS.domS.dominated.add(dstS);
          }
        }
      }
    }

    // 2. Continue revisiting nodes until every node satisfies the requirement
    // that domS.id == oldDomS.id.
    while (!revisit.isEmpty()) {
      NodeS oldDomS = revisit.poll();
      assert oldDomS.revisit != null;

      NodeSet nodes = oldDomS.revisit;
      oldDomS.revisit = null;

      // Search for pairs of nodes nodeS, xS for which
      //    nodeS.id > xS.domS.id > nodeS.oldDomS.id
      // and there is a path of nodes with increasing ids from nodeS to xS.
      // In that case, xS.domS must be wrong, because there is a path to xS
      // from the root that does not go through xS.domS:
      // * There is a path from the root to nodeS.oldDomS that doesn't go
      //   through xS.domS. Otherwise xS.domS would be a dominator of
      //   nodeS.oldDomS, but it can't be because xS.domS.id > nodeS.oldDomS.id.
      // * There is a path from nodeS.oldDomS to nodeS that doesn't go through
      //   xS.domS, because xS.domS is not a dominator of nodeS.
      // * There is a path from nodeS to xS that doesn't go through xS.domS,
      //   because we have a path of increasing ids from nodeS to xS, none of
      //   which can have an id smaller than nodeS as xS.domS does.
      for (int i = 0; i < oldDomS.dominated.size; ++i) {
        NodeS xS = oldDomS.dominated.nodes[i];
        for (int j = 0; j < nodes.size; ++j) {
          NodeS nodeS = nodes.nodes[j];
          assert nodeS.oldDomS == oldDomS;
          if (isReachableAscending(nodeS, xS)) {
            // Update the dominator for xS.
            if (xS.domS == xS.oldDomS) {
              if (xS.oldDomS.revisit == null) {
                xS.oldDomS.revisit = new NodeSet();
                revisit.add(xS.oldDomS);
              }
              xS.oldDomS.revisit.add(xS);
            }
            oldDomS.dominated.remove(i--);
            xS.domS = nodeS.domS;
            xS.domS.dominated.add(xS);
            break;
          }
        }
      }

      // We can now safely update oldDomS for each of the nodes nodeS while
      // preserving the oldDomS invariant.
      for (int i = 0; i < nodes.size; ++i) {
        NodeS nodeS = nodes.nodes[i];
        nodeS.oldDomS = oldDomS.oldDomS;
        if (nodeS.oldDomS != nodeS.domS) {
          if (nodeS.oldDomS.revisit == null) {
            nodeS.oldDomS.revisit = new NodeSet();
            revisit.add(nodeS.oldDomS);
          }
          nodeS.oldDomS.revisit.add(nodeS);
        }
      }
    }

    // 3. We have figured out the correct dominator for each node. Notify the
    // user of the results by doing one last traversal of the nodes.
    assert revisit.isEmpty();
    revisit.add(rootS);
    while (!revisit.isEmpty()) {
      NodeS nodeS = revisit.poll();
      assert nodeS.domS == nodeS.oldDomS;
      assert nodeS.revisit == null;
      nodeS.node.setDominatorsComputationState(null);
      for (int i = 0; i < nodeS.dominated.size; ++i) {
        NodeS xS = nodeS.dominated.nodes[i];
        xS.node.setDominator(nodeS.node);
        revisit.add(xS);
      }
    }
  }

  // Returns true if there is a path from srcS to dstS of nodes with ascending
  // ids (not including dstS.id).
  private static boolean isReachableAscending(NodeS srcS, NodeS dstS) {
    if (dstS.id < srcS.id) {
      // The first time we saw dstS was before we saw srcS. See if srcS is on
      // the source chain for any nodes with direct references to dstS.
      return dstS.inRefIds.hasIdInRange(srcS.id, srcS.maxReachableId);
    }

    // Otherwise dstS is only reachable from srcS on a node with ascending ids
    // if it was visited for the first time while performing the depth-first
    // traversal of srcS.
    return dstS.id <= srcS.maxReachableId;
  }
}

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

import com.android.ahat.progress.NullProgress;
import com.android.ahat.progress.Progress;

/**
 * Provides a static method for computing the immediate dominators of a
 * directed graph. It can be used with any directed graph data structure
 * that implements the {@link DominatorsComputation.Node} interface and has
 * some root node with no incoming edges.
 *
 * @deprecated Use {@link Dominators} class instead, which has a nicer interface.
 */
@Deprecated public class DominatorsComputation {
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
    computeDominators(root, new NullProgress(), 0);
  }

  /**
   * Computes the immediate dominators of all nodes reachable from the <code>root</code> node.
   * There must not be any incoming references to the <code>root</code> node.
   * <p>
   * The result of this function is to call the {@link Node#setDominator}
   * function on every node reachable from the root node.
   *
   * @param root the root node of the dominators computation
   * @param progress progress tracker.
   * @param numNodes upper bound on the number of reachable nodes in the
   *                 graph, for progress tracking purposes only.
   * @see Node
   */
  public static void computeDominators(Node root, Progress progress, long numNodes) {
    Dominators.Graph<Node> graph = new Dominators.Graph<Node>() {
      @Override
      public void setDominatorsComputationState(Node node, Object state) {
        node.setDominatorsComputationState(state);
      }

      @Override
      public Object getDominatorsComputationState(Node node) {
        return node.getDominatorsComputationState();
      }

      @Override
      public Iterable<? extends Node> getReferencesForDominators(Node node) {
        return node.getReferencesForDominators();
      }

      @Override
      public void setDominator(Node node, Node dominator) {
        node.setDominator(dominator);
      }
    };

    new Dominators(graph).progress(progress, numNodes).computeDominators(root);
  }
}

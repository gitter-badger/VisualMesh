#!/usr/bin/env python3

import math
import tensorflow as tf


def build(groups):

    # Number of neighbours for each point
    graph_degree = 7

    # The mesh graph
    G = tf.placeholder(dtype=tf.int32, shape=[None, None, graph_degree], name='MeshGraph')

    # First input is the number of visual mesh points by 3
    X = tf.placeholder(dtype=tf.float32, shape=[None, None, 3], name='Input')

    # Build our tensor
    logits = X
    for i, c in enumerate(groups):
        # Which convolution we are on
        with tf.variable_scope('Conv_{}'.format(i)):

            # The size of the previous output is the size of our input
            prev_last_out = logits.get_shape()[2].value

            with tf.variable_scope('GatherConvolution'):
                net_shape = tf.shape(logits, name='NetworkShape')

                batch_idx = tf.bitcast(tf.reshape(tf.tile(tf.reshape(tf.range(net_shape[0], dtype=tf.int32), [-1, 1]), [1, net_shape[1] * graph_degree * prev_last_out]),
                                    [net_shape[0], net_shape[1], graph_degree * prev_last_out]), tf.float32)

                neighbour_idx = tf.bitcast(tf.reshape(tf.tile(tf.reshape(G, [-1, 1]), [1, prev_last_out]),
                                    [net_shape[0], net_shape[1], graph_degree * prev_last_out]), tf.float32)

                feature_idx = tf.bitcast(tf.reshape(tf.tile(tf.range(prev_last_out, dtype=tf.int32), [net_shape[0] * net_shape[1] * graph_degree]),
                                   [net_shape[0], net_shape[1], graph_degree * prev_last_out]), tf.float32)

                # Now we can use this to lookup our actual network
                network_idx = tf.bitcast(tf.stack([batch_idx, neighbour_idx, feature_idx], axis=3, name='NetworkIndices'), tf.int32)
                logits = tf.gather_nd(logits, network_idx, name='NetworkGather')

            for j, out_s in enumerate(c):

                # Our input size is the previous output size
                in_s = logits.get_shape()[2].value

                # Which layer we are on
                with tf.variable_scope('Layer_{}'.format(j)):

                    # Create weights and biases
                    W = tf.get_variable('Weights', shape=[in_s, out_s], initializer=tf.truncated_normal_initializer(stddev=math.sqrt(2.0/in_s)), dtype=tf.float32)
                    b = tf.get_variable('Biases', shape=[out_s], initializer=tf.truncated_normal_initializer(stddev=math.sqrt(2.0/out_s)), dtype=tf.float32)

                    # Apply our weights and biases
                    logits = tf.tensordot(logits, W, [[2], [0]], name="MatMul")
                    logits = tf.add(logits, b)

                    # Apply our activation function
                    logits = tf.nn.selu(logits)


    # Softmax our final output for all values in the mesh as our network
    network = tf.nn.softmax(logits, name='Softmax', dim=2)

    return {
        'logits': logits,   # Raw unscaled output
        'network': network, # Softmax network output
        'X': X,             # Network input
        'G': G,             # Network graph input
    }


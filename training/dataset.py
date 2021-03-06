#!/usr/bin/env python3

import tensorflow as tf
import os
import re
import math
import multiprocessing

# Load the visual mesh op
op_file = os.path.join(os.path.dirname(__file__), 'visualmesh_op.so')
if os.path.isfile(op_file):
  VisualMesh = tf.load_op_library(op_file).visual_mesh
else:
  raise Exception("Please build the tensorflow visual mesh op before running training")


class VisualMeshDataset:

  def __init__(self, input_files, classes, geometry, batch_size, shuffle_size, variants):
    self.input_files = input_files
    self.classes = classes
    self.batch_size = batch_size
    self.geometry = tf.constant(geometry['shape'], dtype=tf.string, name='GeometryType')
    self.shuffle_buffer_size = shuffle_size

    self.variants = variants

    # Convert our geometry into a set of numbers
    if geometry['shape'] in ['CIRCLE', 'SPHERE']:
      self.geometry_params = tf.constant([geometry['radius'], geometry['intersections'], geometry['max_distance']],
                                         dtype=tf.float32,
                                         name='GeometryParams')

    elif geometry['shape'] in ['CYLINDER']:
      self.geometry_params = tf.constant([
        geometry['height'], geometry['radius'], geometry['intersections'], geometry['max_distance']
      ],
                                         dtype=tf.float32,
                                         name='GeometryParams')
    else:
      raise Exception('Unknown geometry type {}'.format(self.geometry))

  def load_example(self, proto):
    example = tf.parse_single_example(
      proto, {
        'image': tf.FixedLenFeature([], tf.string),
        'mask': tf.FixedLenFeature([], tf.string),
        'lens/projection': tf.FixedLenFeature([], tf.string),
        'lens/focal_length': tf.FixedLenFeature([1], tf.float32),
        'lens/fov': tf.FixedLenFeature([1], tf.float32),
        'mesh/orientation': tf.FixedLenFeature([3, 3], tf.float32),
        'mesh/height': tf.FixedLenFeature([1], tf.float32),
      }
    )

    return {
      'image': tf.image.decode_image(example['image'], channels=3),
      'mask': tf.image.decode_png(example['mask'], channels=4),
      'projection': example['lens/projection'],
      'focal_length': example['lens/focal_length'],
      'fov': example['lens/fov'],
      'orientation': example['mesh/orientation'],
      'height': example['mesh/height'],
      'raw': example['image'],
    }

  def project_mesh(self, args):

    height = args['height']
    orientation = args['orientation']

    # Adjust our height and orientation
    if 'mesh' in self.variants:
      v = self.variants['mesh']
      if 'height' in v:
        height = height + tf.truncated_normal(
          shape=(),
          mean=v['height']['mean'],
          stddev=v['height']['stddev'],
        )
      if 'rotation' in v:
        # Make 3 random euler angles
        rotation = tf.truncated_normal(
          shape=[3],
          mean=v['rotation']['mean'],
          stddev=v['rotation']['stddev'],
        )
        # Cos and sin for everyone!
        ca = tf.cos(rotation[0])
        sa = tf.sin(rotation[0])
        cb = tf.cos(rotation[1])
        sb = tf.sin(rotation[0])
        cc = tf.cos(rotation[2])
        sc = tf.sin(rotation[0])

        # Convert these into a rotation matrix
        rot = [cc*ca, -cc*sa*cb + sc*sb, cc*sa*sb + sc*cb,
                  sa,             ca*cb,         -ca * sb,
              -sc*ca,  sc*sa*cb + cc*sb, -sc*sa*sb + cc*cb]  # yapf: disable
        rot = tf.reshape(tf.stack(rot), [3, 3])

        # Apply the rotation
        orientation = tf.matmul(rot, orientation)

    # Run the visual mesh to get our values
    pixels, neighbours = VisualMesh(
      tf.shape(args['image']),
      args['projection'],
      args['focal_length'],
      args['fov'],
      orientation,
      height,
      self.geometry,
      self.geometry_params,
      name='ProjectVisualMesh',
    )

    # Round to integer pixels
    # TODO one day someone could do linear interpolation here, like what happens in the OpenCL version
    pixels = tf.cast(tf.round(pixels), dtype=tf.int32)

    # Select the points in the network and discard the old dictionary data
    # We pad one extra point at the end for the offscreen point
    return {
      'X': tf.pad(tf.gather_nd(args['image'], pixels), [[0, 1], [0, 0]]),
      'Y': tf.pad(tf.gather_nd(args['mask'], pixels), [[0, 1], [0, 0]]),
      'G': neighbours,
      'px': pixels,
      'raw': args['raw'],
    }

  def flatten_batch(self, args):

    # This adds an offset to the graph indices so they will be correct once flattened
    G = args['G'] + tf.cumsum(args['n'], exclusive=True)[:, tf.newaxis, tf.newaxis]

    # Find the indices of valid points from the mask
    idx = tf.where(tf.squeeze(args['m'], axis=-1))

    # Use this to lookup each of the vectors
    X = tf.gather_nd(args['X'], idx)
    Y = tf.gather_nd(args['Y'], idx)
    G = tf.gather_nd(G, idx)

    return {'X': X, 'Y': Y, 'G': G, 'n': args['n'], 'px': args['px'], 'raw': args['raw']}

  def expand_classes(self, args):

    # Expand the classes from colours into individual columns
    Y = args['Y']
    W = tf.image.convert_image_dtype(Y[:, 3], tf.float32)  # Alpha channel
    cs = []
    for name, value in self.classes:
      cs.append(
        tf.where(
          tf.reduce_all(tf.equal(Y[:, :3], [value]), axis=-1),
          tf.fill([tf.shape(Y)[0]], 1.0),
          tf.fill([tf.shape(Y)[0]], 0.0),
        )
      )
    Y = tf.stack(cs, axis=-1)

    return {
      **args,
      'Y': Y,
      'W': W,
    }

  def apply_variants(self, args):
    # Cast the image to a floating point value and make it into an image shape
    X = tf.expand_dims(tf.image.convert_image_dtype(args['X'], tf.float32), 0)

    # Apply the variants that were listed
    var = self.variants['image']
    if 'brightness' in var and var['brightness']['stddev'] > 0:
      X = tf.image.adjust_brightness(
        X, tf.truncated_normal(
          shape=(),
          mean=var['brightness']['mean'],
          stddev=var['brightness']['stddev'],
        )
      )
    if 'contrast' in var and var['contrast']['stddev'] > 0:
      X = tf.image.adjust_contrast(
        X, tf.truncated_normal(
          shape=(),
          mean=var['contrast']['mean'],
          stddev=var['contrast']['stddev'],
        )
      )
    if 'hue' in var and var['hue']['stddev'] > 0:
      X = tf.image.adjust_hue(X, tf.truncated_normal(
        shape=(),
        mean=var['hue']['mean'],
        stddev=var['hue']['stddev'],
      ))
    if 'saturation' in var and var['saturation']['stddev'] > 0:
      X = tf.image.adjust_saturation(
        X, tf.truncated_normal(
          shape=(),
          mean=var['saturation']['mean'],
          stddev=var['saturation']['stddev'],
        )
      )
    if 'gamma' in var and var['gamma']['stddev'] > 0:
      X = tf.image.adjust_gamma(
        X, tf.truncated_normal(
          shape=(),
          mean=var['gamma']['mean'],
          stddev=var['gamma']['stddev'],
        )
      )

    return {**args, 'X': tf.squeeze(tf.image.convert_image_dtype(X, tf.uint8), 0)}

  def build(self):
    # Load our dataset
    dataset = tf.data.TFRecordDataset(self.input_files)
    dataset = dataset.map(self.load_example, num_parallel_calls=multiprocessing.cpu_count())

    # Before we get to the point of making variants etc, shuffle here
    if self.shuffle_buffer_size > 0:
      dataset = dataset.shuffle(buffer_size=self.shuffle_buffer_size)

    # Project the visual mesh and select the appropriate pixels
    dataset = dataset.map(self.project_mesh, num_parallel_calls=multiprocessing.cpu_count())

    # Batch the visual mesh by concatenating meshes and graphs and updating the graph coordinates to match
    dataset = dataset.map(
      lambda args: {
        **args,
        'n': tf.shape(args['X'])[0],
        'm': tf.fill((tf.shape(args['X'])[0], 1), True)}
    )
    dataset = dataset.prefetch(self.batch_size * 2)
    dataset = dataset.padded_batch(
      batch_size=self.batch_size,
      padded_shapes={
        'X': (None, 3),
        'Y': (None, 4),
        'G': (None, 7),
        'n': (),
        'm': (None, 1),
        'px': (None, 2),
        'raw': (),
      },
    )
    dataset = dataset.map(self.flatten_batch, num_parallel_calls=multiprocessing.cpu_count())

    # Expand the classes
    dataset = dataset.map(self.expand_classes, num_parallel_calls=multiprocessing.cpu_count())

    # Apply visual variations to the data
    if 'image' in self.variants:
      dataset = dataset.map(self.apply_variants, num_parallel_calls=multiprocessing.cpu_count())

    # Finally, convert our image to float
    dataset = dataset.map(lambda args: {**args, 'X': tf.image.convert_image_dtype(args['X'], tf.float32)}, num_parallel_calls=multiprocessing.cpu_count())

    # And prefetch 2
    dataset = dataset.prefetch(2)

    return dataset

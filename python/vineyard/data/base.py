#! /usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2020-2022 Alibaba Group Holding Limited.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import re

import numpy as np

from vineyard._C import Object
from vineyard._C import ObjectID
from vineyard._C import ObjectMeta

from .tensor import ndarray
from .utils import normalize_dtype


def int_builder(client, value, **kwargs):
    meta = ObjectMeta(**kwargs)
    meta['typename'] = 'vineyard::Scalar<int>'
    meta['value_'] = value
    meta['type_'] = getattr(type(value), '__name__')
    meta['nbytes'] = 0
    return client.create_metadata(meta)


def double_builder(client, value, **kwargs):
    meta = ObjectMeta(**kwargs)
    meta['typename'] = 'vineyard::Scalar<double>'
    meta['value_'] = value
    meta['type_'] = getattr(type(value), '__name__')
    meta['nbytes'] = 0
    return client.create_metadata(meta)


def string_builder(client, value, **kwargs):
    meta = ObjectMeta(**kwargs)
    meta['typename'] = 'vineyard::Scalar<std::string>'
    meta['value_'] = value
    meta['type_'] = getattr(type(value), '__name__')
    meta['nbytes'] = 0
    return client.create_metadata(meta)


def bytes_builder(client, value, **_kwargs):
    buffer = client.create_blob(len(value))
    buffer.copy(0, value)
    return buffer.seal(client)


def memoryview_builder(client, value, **_kwargs):
    buffer = client.create_blob(len(value))
    buffer.copy(0, bytes(value))
    return buffer.seal(client)


def sequence_builder(client, value, builder, **kwargs):
    meta = ObjectMeta(**kwargs)
    meta['typename'] = 'vineyard::Sequence'
    meta['size_'] = len(value)
    for i, item in enumerate(value):
        if isinstance(item, (ObjectID, Object, ObjectMeta)):
            meta.add_member('__elements_-%d' % i, item)
        else:
            meta.add_member('__elements_-%d' % i, builder.run(client, item))
    meta['__elements_-size'] = len(value)
    return client.create_metadata(meta)


def scalar_resolver(obj):
    meta = obj.meta
    typename = obj.typename
    if typename == 'vineyard::Scalar<std::string>':
        return meta['value_']
    if typename == 'vineyard::Scalar<int>':
        return int(meta['value_'])
    if typename in ['vineyard::Scalar<float>', 'vineyard::Scalar<double>']:
        return float(meta['value_'])
    return None


def bytes_resolver(obj):
    return memoryview(obj)


def sequence_resolver(obj, resolver):
    meta = obj.meta
    elements = []
    for i in range(int(meta['__elements_-size'])):
        elements.append(resolver.run(obj.member('__elements_-%d' % i)))
    return tuple(elements)


def array_resolver(obj):
    typename = obj.typename
    value_type = normalize_dtype(
        re.match(r'vineyard::Array<([^>]+)>', typename).groups()[0]
    )
    return np.frombuffer(memoryview(obj.member("buffer_")), dtype=value_type).view(
        ndarray
    )


def register_base_types(builder_ctx=None, resolver_ctx=None):
    if builder_ctx is not None:
        builder_ctx.register(int, int_builder)
        builder_ctx.register(float, double_builder)
        builder_ctx.register(str, string_builder)
        builder_ctx.register(tuple, sequence_builder)
        builder_ctx.register(list, sequence_builder)
        builder_ctx.register(bytes, bytes_builder)
        builder_ctx.register(memoryview, memoryview_builder)

    if resolver_ctx is not None:
        resolver_ctx.register('vineyard::Blob', bytes_resolver)
        resolver_ctx.register('vineyard::Scalar', scalar_resolver)
        resolver_ctx.register('vineyard::Array', array_resolver)
        resolver_ctx.register('vineyard::Sequence', sequence_resolver)

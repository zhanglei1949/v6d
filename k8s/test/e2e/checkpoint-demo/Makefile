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

REGISTRY := docker.pkg.github.com/v6d-io/v6d
TAG      := latest

all: build-spill build-serialize docker-push
	
build-spill:
	cd ../../../../ && docker build -f k8s/test/e2e/checkpoint-demo/Dockerfile.spill -t docker.pkg.github.com/v6d-io/v6d/spill:latest .
.PHONY: build-spill

build-serialize:
	docker build . -f Dockerfile.serialize -t $(REGISTRY)/serialize:$(TAG)
.PHONY: build-serialize

docker-push: build-spill build-serialize
	docker push $(REGISTRY)/serialize:$(TAG)
	docker push $(REGISTRY)/spill:$(TAG)
.PHONY: docker-push
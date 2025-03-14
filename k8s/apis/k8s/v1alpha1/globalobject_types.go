/** Copyright 2020-2022 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package v1alpha1

import (
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// GlobalObjectSpec defines the desired state of GlobalObject
type GlobalObjectSpec struct {
	ObjectID  string   `json:"id"`
	Name      string   `json:"name,omitempty"`
	Signature string   `json:"signature"`
	Typename  string   `json:"typename,omitempty"`
	Members   []string `json:"members"`
	Metadata  string   `json:"metadata"`
}

// GlobalObjectStatus defines the observed state of GlobalObject
type GlobalObjectStatus struct {
}

// +kubebuilder:object:root=true
// +kubebuilder:resource:categories=all,shortName=gobject
// +kubebuilder:subresource:status
// +kubebuilder:printcolumn:name="Id",type=string,JSONPath=`.spec.id`
// +kubebuilder:printcolumn:name="Name",type=string,JSONPath=`.spec.name`
// +kubebuilder:printcolumn:name="Signature",type=string,JSONPath=`.spec.signature`
// +kubebuilder:printcolumn:name="Typename",type=string,JSONPath=`.spec.typename`
// +genclient

// GlobalObject is the Schema for the globalobjects API
type GlobalObject struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   GlobalObjectSpec   `json:"spec,omitempty"`
	Status GlobalObjectStatus `json:"status,omitempty"`
}

// +kubebuilder:object:root=true

// GlobalObjectList contains a list of GlobalObject
type GlobalObjectList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []GlobalObject `json:"items"`
}

func init() {
	SchemeBuilder.Register(&GlobalObject{}, &GlobalObjectList{})
}

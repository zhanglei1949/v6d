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
	"k8s.io/apimachinery/pkg/runtime"
	ctrl "sigs.k8s.io/controller-runtime"
	logf "sigs.k8s.io/controller-runtime/pkg/log"
	"sigs.k8s.io/controller-runtime/pkg/webhook"
)

// log is for logging in this package.
var globalobjectlog = logf.Log.WithName("globalobject-resource")

// SetupWebhookWithManager implements the webhook.Defaulter so a webhook will be registered
func (r *GlobalObject) SetupWebhookWithManager(mgr ctrl.Manager) error {
	return ctrl.NewWebhookManagedBy(mgr).
		For(r).
		Complete()
}

//nolint: lll
//+kubebuilder:webhook:path=/mutate-k8s-v6d-io-v1alpha1-globalobject,mutating=true,failurePolicy=fail,sideEffects=None,groups=k8s.v6d.io,resources=globalobjects,verbs=create;update,versions=v1alpha1,name=mglobalobject.kb.io,admissionReviewVersions=v1

var _ webhook.Defaulter = &GlobalObject{}

// Default implements webhook.Defaulter so a webhook will be registered for the type
func (r *GlobalObject) Default() {
	globalobjectlog.Info("default", "name", r.Name)

	// TODO(user): fill in your defaulting logic.
}

//nolint: lll
//+kubebuilder:webhook:path=/validate-k8s-v6d-io-v1alpha1-globalobject,mutating=false,failurePolicy=fail,groups=k8s.v6d.io,resources=globalobjects,verbs=create;update,versions=v1alpha1,admissionReviewVersions=v1,sideEffects=None,name=vglobalobject.kb.io

var _ webhook.Validator = &GlobalObject{}

// ValidateCreate implements webhook.Validator so a webhook will be registered for the type
func (r *GlobalObject) ValidateCreate() error {
	globalobjectlog.Info("validate create", "name", r.Name)

	return nil
}

// ValidateUpdate implements webhook.Validator so a webhook will be registered for the type
func (r *GlobalObject) ValidateUpdate(old runtime.Object) error {
	globalobjectlog.Info("validate update", "name", r.Name)

	return nil
}

// ValidateDelete implements webhook.Validator so a webhook will be registered for the type
func (r *GlobalObject) ValidateDelete() error {
	globalobjectlog.Info("validate delete", "name", r.Name)

	return nil
}

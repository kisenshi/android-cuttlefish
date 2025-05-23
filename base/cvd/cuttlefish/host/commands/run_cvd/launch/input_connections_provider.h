//
// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <vector>

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/host/commands/run_cvd/launch/log_tee_creator.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/feature/feature.h"

namespace cuttlefish {

// Feature that provides access to the connections to the input devices.
// Such connections are file descriptors over which (virtio_) input events can
// be written to inject them to the VM and (virtio_) status updates can be read.
class InputConnectionsProvider : public virtual SetupFeature {
 public:
  virtual ~InputConnectionsProvider() = default;

  virtual SharedFD RotaryDeviceConnection() const = 0;
  virtual SharedFD MouseConnection() const = 0;
  virtual SharedFD KeyboardConnection() const = 0;
  virtual SharedFD SwitchesConnection() const = 0;
  virtual std::vector<SharedFD> TouchscreenConnections() const = 0;
  virtual std::vector<SharedFD> TouchpadConnections() const = 0;
};

fruit::Component<fruit::Required<const CuttlefishConfig::InstanceSpecific>,
                 InputConnectionsProvider, LogTeeCreator>
VhostInputDevicesComponent();

}  // namespace cuttlefish

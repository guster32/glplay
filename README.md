# glplay


##Original Flow

```mermaid
sequenceDiagram
    main->>device_create: device_create
    device_create->>device_open: device_open
        loop connector
        device_open->>device_open: output_create
    end
    device_open->>gbm_create_device:gbm_create_device
    device_open->>device_egl_setup:device_egl_setup

```

EGL is not is not fully formed yet. Currently its just a collection of functions. These functions are used by `DisplayAdapter`

Currently working through the call flow `device_create` -> `device_open` -> `device_egl_setup`
Next we need to crr


##Overview of new Structure
```mermaid
classDiagram
class DisplayAdapter{
    displays~Display~
}

class Display 
```

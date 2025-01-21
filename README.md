# Test Raspberry Pi Pico W (RP2040) Access Point To html application

I started with this: https://github.com/gherlein/pico-ap-c-test/blob/main/README.md

I am trying to build a pico-w IOT device. One of the biggest problems is
initializing a IOT system in a random wifi environment. So my first thought
was to do a bluetooth interface to set my ssid/password/ip/ipmask and then
use my local wifi. I implemented a standalone bluetooth project here:
https://github.com/nospamcalfee/spp_in_out But the downsides is apparently
apple ios doesn't respond to serial protocol bluetooth (rumor, I have not
tried this), and it uses lots of flash - 1/4 total which is not available for
the final app.

I did find that lwip would support mdns, which will make dhcp assigned ip
addresses easier to find. See:
https://github.com/nospamcalfee/picow_mdns_webserver for a minimal example.


The first step was to do a generic, small footprint, target somewhat
independent (easily ported?), flash file system handler. See
https://github.com/nospamcalfee/ringbuffer

Next steps are initing a system for ssid/password/ip/mask. That is this
project.

## What this example does

First the pico-w starts up as an access point so a cellphone or pc can join
the pico-w network. Once this happens the user can use a browser to connect
to the pico-w ap website at default 192.168.4.1 and then the user can set
ssid/password and optionally fixed-ip and fixed-netmask.

I took the gherlein app and made it standalone. Then expanded to include
entering SSID and password and optional IP and Mask. Then I added POST
processing which I needed for my app, and is already needed for the AP
ssid etc entry anyway.

The cgi POST processing is already in the sdk, but the post_example.c is
pretty vague. The prime example in this project is how POST works and how an
AP works.

### still to be done.

In many iot apps, especially ones that do not use mdns, each device needs to
be configured - setting ssid and password as here, but also setting a fixed
IP/Mask to be used. I have not done that yet, maybe later, but it is a simple
extension of this example's POST handling.

### Tricks to use CGI-POST handling

LWIP is very simple minded. The complication is that your html must call the
correct cgi entry for the post server. Then the post handler must know what
is passed by the html such as ssid or ip etc.

CGI GET handling is already handled internal to the httpd.c. You must add the
POST handling at the lowest level.

## Build and Install

This build assumes you have the pico-sdk installed and have defined in your
environment PICO_SDK_PATH. This package works as a native Linux shell build,
but should be easy to people who wish to further complicate the build using a
ide environment.

There is no requirement for any pico-examples source code (I hope).

Builds default to release in the pico-sdk - but CMakeLists.txt overrides this,
and still needs the -DCMAKE_BUILD_TYPE=Debug in the cmake incantation. You
can change to release by fixing CMakeLists.txt and not doing
the -DCMAKE_BUILD_TYPE stuff.

To completely rebuild a project including the cmake, before typing the
cmake ... step first enter "rm CMakeCache.txt", or whatever complications
your IDE requires.

```bash
cd yourprojectdirectory
mkdir build; cd build
cmake  -DWIFI_SSID="yourwifi" -DWIFI_PASSWORD="1234567890" -DHOSTNAME="test" -DCMAKE_BUILD_TYPE=Debug ..
make
```

Once built, the `picow_access_point.uf2` file can be dragged and dropped onto your
Raspberry Pi Pico W to install and run the example.


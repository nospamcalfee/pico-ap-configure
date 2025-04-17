# Test Raspberry Pi Pico W (RP2040) Access Point To html application

I started with this: https://github.com/gherlein/pico-ap-c-test/blob/main/README.md

I am trying to build a pico-w IOT device. One of the biggest problems is
initializing a IOT system in a random wifi environment. So my first thought
was to do a bluetooth interface to set my ssid/password/ip/ipmask and then
use my local wifi. I implemented a standalone bluetooth project here:
https://github.com/nospamcalfee/spp_in_out But the downsides is apparently
apple ios doesn't respond to serial protocol bluetooth (rumor, I have not
tried this), and the bluetooth build uses lots of flash - 1/4 total which is
not available for the final app.

I did find that lwip would support mdns, which will make dhcp assigned ip
addresses easier to find. See:
https://github.com/nospamcalfee/picow_mdns_webserver for a minimal example.


The first step was to do a generic, small footprint, target somewhat
independent (easily ported?), flash file system handler. See
https://github.com/nospamcalfee/ringbuffer

Next step is initing a system for ssid/password/ip/mask. I also incorporated
the mdns stuff in this test.
https://github.com/nospamcalfee/pico-ap-configure


## What this example does


From the code:

```
        Starting up, first we see if we have any local wifi ssids that match
        known ones in flash. If we cannot connect to any of these we become
        an AP and get the user to give us the local wifi credentials. If no
        ap creds are supplied, we time out and try again in the hope that a
        missing AP may turn up (due to power failure).

        details:

         once started, the wifi AP might not be up yet. So maybe we have to do another
         find of all available if all possible connections fails.

         The sequence is

         OUTER LOOP

         1) get a list of all APs

         INNER LOOP
             2) find the most powerful remaining ap in the list.

             3) see if the flash has an entry for this high power ap, if not in flash, go
             back to step 2 (Inner loop)

             4) if connection fails, retry from step 2 until all discovered and known APs
             have been tried.

         5) if connection still fails after all the known APs have been tried, become
         an AP to see if the user wants to configure wifi.

         6) after AP handling, which may timeout or may set a new flash AP entry,
         start over at step 1.


```

When the application cannot connect to a known wifi, the pico-w starts up as
an access point so a cellphone or pc can join the pico-w network. Once this
happens the user can use a browser to connect to the pico-w ap website at
default 192.168.4.1 and then the user can set ssid/password and optionally
fixed-ip and fixed-netmask.

This is operationally difficult for most users who do this occasionally. So a
long timeout of 3 minutes is allowed before the iot device retries to connect
to any known wifi networks. This helps if a power failure resets both the iot
device and an AP which may take a long time to boot up. So unattended iot
devices will eventually connect to a slow AP.

First the connecting device (phone or pc) must connect to the temporary ap of
the picow. In an IOT situation maybe several devices need to be configured,
so maybe to make this easier an unnamed device id named webapp_01_9a_bb where
the funny numbers are the low 3 bytes of the built-in MAC address.

Second they must enter their local wifi name and sometimes complicated wifi
passwords.

Finally, the connecting device has to have a browser connected on this new
network to 192.168.4.1

I took the gherlein app and made it standalone. Then expanded to include
entering SSID and password. Then I added cgi POST processing which I needed
for my app, and is already needed for the AP ssid/password/hostname entry
anyway.

The cgi POST processing is already in the sdk, but the post_example.c is
pretty vague. A prime example in this project is how POST works and how an AP
works.

The build requirement for WIFI_SSID and WIFI_PASSWORD and HOSTNAME is removed.
Now, if no local wifi credentials are in flash, the user (you!) has to go
into the AP website and define the ssid/password/hostname. Then the system
should reconnect.

After the app starts it looks for active local wifi ssids, if a matching ssid
exists in flash, that is used for a connect. After timeout it will start the
access point (by default name ```webapp_xx_xx_xx``` where xx is a sort of
random hex number). The default network password is cleverly ```password```.
Your own local iot network name and password can be changed in the function
be_access_point. After connecting to your ap, you can then access the html in
a browser at 192.168.4.1 and set the actual wifi network credentials you have
locally.

I extended this example to include mdns (AKA bonjour or avahi). I was
surprised to see a panic from lwip. Searching around the forums I see
recommendations to increase lwipopts.h timers. But I already had the
recommended setting, so I increased it again.

```#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL+4)```

Now it seems to work. I realize that lwip is an external project, but people
who do api's to be used by others should use the standard embedded C
programmers rule:

```There are only 3 numbers of interest to embedded programmers, 0, 1 and many ```

This means that arrays should be as few as possible to minimize configuration
issues like this. Lists should be used for changeable numbers depending on
configuration and usage.

### still to be done.

In many iot apps, especially ones that do not use mdns, each device needs to
be configured - setting ssid and password as here, but also setting a fixed
IP/Mask to be used. I have not done that yet, maybe later, but it is a simple
extension of this example's POST handling.

If I don't get ntp time from a server, I cannot do regular IOT timing.
Problems need to be reported on the web page - like no ntp or no comm.

I need watchdog reset - like an arcade game, the only big sin is not not be
ready for a coin or in this case IOT time handling.

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
the ```-DCMAKE_BUILD_TYPE``` stuff.

New, a change to CMakeLists.txt makes editing changes to the listed website
html/etc causes files to be rebuilt when altered. New files to be added to
the pico website will require them to be individually listed in
CMakeLists.txt.

```bash
cd yourprojectdirectory
mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Once built, the `picow_webapp.uf2` file can be dragged and dropped onto your
Raspberry Pi Pico W to install and run the example. If your network
ssid/password/hostname already in flash are correct, or after you set them in
the AP mode, you can then access the application html using the mdns name
(in this cmake hostname example) as test.local. You will have to use a mdns
browser on android, something on iphones or windows to find a new, never
before setup device, probably with some variant of webapp_xx_xx_xx . Once you
set the hostname with the built in AP, that name will be used in any browser
such as test.local

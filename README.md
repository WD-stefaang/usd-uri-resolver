# USD-S3-resolver
An S3 object store based PackageResolver plugin for USD. Based on LumaPictures's URIResolver.

## Project Goals
* Support assets in S3 buckets by referencing them in USD as @s3:bucket/object.usd@
* Support S3 versioning

## Features
* Cache USD files to local directory, defaults to /tmp/bucket/object. Change it with USD_S3_CACHE_PATH environment variable
* Connect to ActiveScale S3 by using environment vars USD_S3_PROXY_HOST and USD_S3_PROXY_PORT.

## Building

You'll need the following libraries to build the project; newer versions most likely work as well, but they are not tested. Currently, the cmake files are looking for the openexr libs, but only use the half headers. This will be removed in the future.

| Package           | Version        |
| ----------------- | -------------- |
| USD               | 0.7.6+ (stock) |
| TBB               | 4.3+           |
| OpenEXR           | 2.2.0+         |
| Boost             | 1.61.0+        |
| CMAKE             | 3.1+           |

Get the [AWS sdk for C++](https://github.com/aws/aws-sdk-cpp) as follows
```
cd src
git clone https://github.com/aws/aws-sdk-cpp
cd ../build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3" ../src/aws-sdk-cpp/
make && sudo make install
```

There are two main ways to configure a library location. 1, configure an environment variable. 2, pass the location of the library using -D\<varname\>=\<path\> to cmake. This will be simplified soon, once we add proper find modules.

* Point the USD\_ROOT environment variable to the location of the installed USD.
* Pass OPENEXR\_LOCATION to the CMake command or setup the OPENEXR\_LOCATION environment variable. They have to point at a standard build of OpenEXR, including IlmBase.
* Point TBB\_ROOT\_DIR}, TBB\_INSTALL\_DIR or TBBROOT at your local TBB installation.

## Using the S3 resolver.
Get the AWS cli.
```
virtualenv venv
venv/bin/activate
pip install awscli
```
Get the S3 credentials from the ActiveScale system node.
Now configure the cli (creating `~/.aws/credentials`)
```
aws configure
alias s3="aws s3 --endpoint-url http://systemnode-ip"
```
Get some usd assets, e.g. [here](http://graphics.pixar.com/usd/downloads.html).
```
s3 mb s3://hello
s3 cp kitchen.usdz s3://hello/kitchen.usdz
```
Set environment variable PXR_PLUGINPATH_NAME to the path with the S3Resolver plugInfo.json file.
Now use the usd tools such as usdview, usdcat, ...
```
usdview s3:hello/kitchen.usdz
```
See .vscode/tasks.json for an example.

For more info, consult the README.md installed alongside the S3Resolver.


## Contributing
TODO.
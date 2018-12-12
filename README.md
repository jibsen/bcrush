
bcrush
======

[![Build Status](https://dev.azure.com/jibsenorg/jibsen/_apis/build/status/jibsen.bcrush?branchName=master)](https://dev.azure.com/jibsenorg/jibsen/_build/latest?definitionId=2?branchName=master)

About
-----

This is an example using some of the compression algorithms from [BriefLZ][]
to produce output in the format of [CRUSH][] by Ilya Muravyov.

**Please note:** this is just a quick experiment to see how it would work, it
is not production quality, and has not been properly tested.

[BriefLZ]: https://github.com/jibsen/brieflz
[CRUSH]: https://sourceforge.net/projects/crush/


Benchmark
---------

Here are some results on the [Silesia compression corpus][silesia]:

| File    |   Original | `bcrush --optimal` | `crush cx` | `crushx -9` |
| :------ | ---------: | -----------------: | ---------: | ----------: |
| dickens | 10.192.446 |          3.148.963 |  3.350.093 |   3.343.930 |
| mozilla | 51.220.480 |         18.037.611 | 18.760.573 |  18.281.301 |
| mr      |  9.970.564 |          3.367.533 |  3.532.785 |   3.428.968 |
| nci     | 33.553.445 |          2.407.286 |  2.624.037 |   2.750.658 |
| ooffice |  6.152.192 |          2.832.224 |  2.958.518 |   2.871.884 |
| osdb    | 10.085.684 |          3.424.687 |  3.545.632 |   3.457.335 |
| reymont |  6.627.202 |          1.523.547 |  1.644.701 |   1.610.306 |
| samba   | 21.606.400 |          4.720.964 |  4.912.141 |   4.911.613 |
| sao     |  7.251.944 |          5.344.713 |  5.472.035 |   5.368.466 |
| webster | 41.458.703 |          9.766.251 | 10.430.228 |  10.322.130 |
| xml     |  5.345.280 |          5.717.405 |  5.958.603 |   5.747.141 |
| x-ray   |  8.474.240 |            535.316 |    563.744 |     561.118 |

Where crush is the original CRUSH v1.00, and crushx is an implementation of
crush with optimal parsing [posted][crushx] on Encode's Forum.

[silesia]: http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia
[crushx]: https://encode.ru/threads/2578-crush-v1-1


Usage
-----

bcrush uses [Meson][] to generate build systems. To create one for the tools on
your platform, and build bcrush, use something along the lines of:

~~~sh
mkdir build
cd build
meson ..
ninja
~~~

You can also simply compile and link the source files.

bcrush includes the leparse and ssparse algorithms from BriefLZ, which gives
compression levels `-5` to `-9` and the **very** slow `--optimal`.

[Meson]: https://mesonbuild.com/


Notes
-----

  - The CRUSH format does not store the size of the compressed block, so I
    copied the way the CRUSH depacker reads one byte at a time from the file
    to avoid issues with reading the next block into memory.
  - bcrush only hashes 3 bytes to find matches, which makes it slow on files
    with many small matches. It might benefit from using two hash tables like
    CRUSH.


License
-------

This projected is licensed under the [zlib License](LICENSE) (Zlib).

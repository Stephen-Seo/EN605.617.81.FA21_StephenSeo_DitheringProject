# Dithering with OpenCL

## Note about Maintenance and Versions

The project at tag `1.0.0` refers to the state of the project upon completion
of the course. Newer versions are meant to be maintenance releases; fixes that
keep the project working. As of 2022-01-18, there may be (after now) a version
`1.1.0` or `1.0.1` due to keeping up to date with the FFmpeg dependency as a new
major version of FFmpeg was released (version `5.0`).

## Background

[Project Proposal PDF](https://seodisparate.com/static/uploads/EN605.617.81.FA21_ProjectProposal.pdf)

The proposal mentioned dithering live-input frames, but I ended up with just
dithering single images or turning a video into dithered images.

The "develop" branch may have more up to date code.

# Usage

A "blue-noise" image is integral to dithering images. A generated blue-noise
image is provided in the "res/" directory. Run the program with "--help" to get
info on how to use it.

Blue-noise images were generated using [my other project that generates
blue-noise with/without OpenCL](https://git.seodisparate.com/stephenseo/blue_noise_generation).

PNG, PGM, and PPM image formats are supported.

For decoding video, any format that ffmpeg can read should work (though if
things don't work, try using MP4 files).

~~WARNING: Video decoding is still a WIP. The video is currently decoded, but an
output video being encoded hasn't been implemented yet. The current
implementation writes each video frame to a PNG image in the current
directory.~~

Video decoding and encoding is finished, ~~but there is some noticable drops in
quality when encoding to colored dithered video~~ but the resulting video may
end up being too large. It may be better to just output each frame to individual
PNGs, then combining them later just like as mentioned here with the params you
want set: https://trac.ffmpeg.org/wiki/Slideshow .

# Other Notes

~~I plan on adding the MIT License to this project once the course (that this
project was made for) is over.~~

I went ahead and applied the MIT License earlier than noted here.

## C++ Compatability

The source is written to work with C++11 but not any higher. The only
significant part that this affects are the functions in the "Image" class that
return an optional value (non-optional on success). Ideally, C++17's
std::optional would be used in that case, but since I am keeping things to work
up to C++11, std::unique\_ptr is used instead.

# Project Presentation

[Click here to view the presentation](https://igpup.seodisparate.com/posts/project_presentation/)

# Legal Stuff

[This program uses FFmpeg which is licensed under the LGPL 2.1 license](https://ffmpeg.org/legal.html)

[This program uses libpng which is licensed under the PNG Reference Library License Version 2](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt)

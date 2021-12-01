# Dithering with OpenCL

## Background

[Project Proposal PDF](https://seodisparate.com/static/uploads/EN605.617.81.FA21_ProjectProposal.pdf)

The proposal mentioned dithering live-input frames, but I ended up with just
dithering single images or turning a video into dithered images.

The "develop" branch may have more up to date code.

# Usage

A "blue-noise" image is integral to dithering images. A generated blue-noise
image is provided in the "res/" directory. Run the program with "--help" to get
info on how to use it.

PNG, PGM, and PPM image formats are supported.

For decoding video, any format that ffmpeg can read should work (though if
things don't work, try using MP4 files).

# Other Notes

~~I plan on adding the MIT License to this project once the course (that this
project was made for) is over.~~

I went ahead and applied the MIT License earlier than noted here.

# Legal Stuff

[This program uses FFmpeg which is licensed under the LGPL 2.1 license](https://ffmpeg.org/legal.html)

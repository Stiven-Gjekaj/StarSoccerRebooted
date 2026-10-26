# Third party code

The files come from <https://github.com/nothings/stb> at commit
`003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`. The license is MIT or public
domain, as the end of each file tells. Do not change the files. Put each
change in the code that uses them.

- `stb_image.h`: the image loader of Sean Barrett, version 2.30. The app
  uses it for the PNG and JPG images of the game. The SHA-256 of the file
  is `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.
- `stb_image_write.h`: the image writer, version 1.16. Only the tests use
  it, to make the images that they need. The SHA-256 of the file is
  `cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05`.
- `stb_truetype.h`: the TrueType rasterizer, version 1.26. The app uses
  it for the TTF fonts of the game. The SHA-256 of the file is
  `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`.
- `stb_vorbis.c`: the Ogg Vorbis decoder, version 1.22, from the same
  commit of stb. The app uses it for the sounds of an APK. The SHA-256 of
  the file is `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`.
- `dr_mp3.h`: the MP3 decoder of David Reid, version 0.7.4 (not yet
  released). It comes from <https://github.com/mackron/dr_libs> at commit
  `a466dbe88fe2576d46c43877d602f3d66b8561a7`. The license is public
  domain or MIT-0. The app uses it for the sounds of an IPA. The SHA-256 of
  the file is `390e4e0ef0cff80fb86ebf1b461d6947cfa4ed699a4fe107e5a5e69a47db2950`.

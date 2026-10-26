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

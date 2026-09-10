`records-v1.bin` is a 200-byte save encoded independently of the current game:
magic 0x4752564c, version 1, car 2, track 1, with Torr/Bracken cumulative times
18/36/54/74/92 seconds and all other pairs empty. Words are little-endian; the
final word is the original FNV-1a checksum of the preceding 196 bytes. It verifies
migration of actual old-format bytes, including global unlock credit.

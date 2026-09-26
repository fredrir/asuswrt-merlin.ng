import importlib.util
from pathlib import Path
import struct
import unittest
import zlib


spec = importlib.util.spec_from_file_location('check_image', Path(__file__).parents[1] / 'check-image.py')
image = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image)


def firmware(model=b'TUF-BE9400', arch=22, payload=b'example firmware payload'):
    name = bytes((3, 0, 0, 6)) + model.ljust(12, b'\0') + bytes(16)
    header = image.HEADER.pack(0x27051956, 0, 1, len(payload), 0x40080000,
                               0x40080000, zlib.crc32(payload), 5, arch, 2, 3, name)
    return header[:4] + struct.pack('>I', zlib.crc32(header)) + header[8:] + payload


class ImageChecks(unittest.TestCase):
    def test_accepts_expected_hardware_header_without_claiming_flash_compatibility(self):
        result = image.check_image(firmware())
        self.assertEqual(result['model'], 'TUF-BE9400')
        self.assertEqual(result['flash_compatibility'], 'not established')

    def test_rejects_other_boards_even_with_valid_checksums(self):
        with self.assertRaisesRegex(ValueError, 'product ID'):
            image.check_image(firmware(model=b'TUF-BE6500'))

    def test_rejects_other_cpu_architectures(self):
        with self.assertRaisesRegex(ValueError, 'AArch64'):
            image.check_image(firmware(arch=2))

    def test_rejects_header_corruption(self):
        data = bytearray(firmware())
        data[8] ^= 1
        with self.assertRaisesRegex(ValueError, 'Header CRC32'):
            image.check_image(data)

    def test_rejects_payload_corruption(self):
        data = bytearray(firmware())
        data[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'Payload CRC32'):
            image.check_image(data)

    def test_rejects_truncation_and_extra_data(self):
        data = firmware()
        for invalid in (b'', data[:63], data[:-1], data + b'\0'):
            with self.subTest(length=len(invalid)), self.assertRaises(ValueError):
                image.check_image(invalid)

    def test_rejects_empty_payload(self):
        with self.assertRaisesRegex(ValueError, 'length'):
            image.check_image(firmware(payload=b''))

    def test_rejects_image_larger_than_observed_ubi_volume(self):
        data = firmware(payload=b'\0' * image.LINUX_VOLUME_BYTES)
        with self.assertRaisesRegex(ValueError, 'UBI volume'):
            image.check_image(data)


if __name__ == '__main__':
    unittest.main()

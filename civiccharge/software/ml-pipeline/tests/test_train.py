import unittest
from train import sample

class SyntheticTests(unittest.TestCase):
    def test_seed_reproducible(self):
        self.assertEqual(sample(), sample())
        self.assertEqual(len(sample(n=3)), 3)

if __name__ == "__main__": unittest.main()

import unittest
import itertools
from unittest import TestCase
from metadata_model import *
from metadata_helpers import *

class TestHelpers(TestCase):

  def test_enum_calculate_value_string(self):
    def compare_values_against_list(expected_list, enum):
      for (idx, val) in enumerate(expected_list):
        self.assertEquals(val,
                          enum_calculate_value_string(list(enum.values)[idx]))

    plain_enum = Enum(parent=None, values=['ON', 'OFF'])

    compare_values_against_list(['0', '1'],
                                plain_enum)

    ###
    labeled_enum = Enum(parent=None, values=['A', 'B', 'C'], ids={
      'A': '12345',
      'B': '0xC0FFEE',
      'C': '0xDEADF00D'
    })

    compare_values_against_list(['12345', '0xC0FFEE', '0xDEADF00D'],
                                labeled_enum)

    ###
    mixed_enum = Enum(parent=None,
                      values=['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'],
                      ids={
                        'C': '0xC0FFEE',
                        'E': '123',
                        'G': '0xDEADF00D'
                      })

    expected_values = ['0', '1', '0xC0FFEE', '0xC0FFEF', '123', '124',
                       '0xDEADF00D',
                       '0xDEADF00E']

    compare_values_against_list(expected_values, mixed_enum)

  def test_enumerate_with_last(self):
    empty_list = []

    for (x, y) in enumerate_with_last(empty_list):
      self.fail("Should not return anything for empty list")

    single_value = [1]
    for (x, last) in enumerate_with_last(single_value):
      self.assertEquals(1, x)
      self.assertEquals(True, last)

    multiple_values = [4, 5, 6]
    lst = list(enumerate_with_last(multiple_values))
    self.assertListEqual([(4, False), (5, False), (6, True)], lst)

  def test_wbr(self):
    wbr_string = "<wbr/>"
    wbr_gen = itertools.repeat(wbr_string)

    # No special characters, do nothing
    self.assertEquals("no-op", wbr("no-op"))
    # Insert WBR after characters in [ '.', '/', '_' ]
    self.assertEquals("word.{0}".format(wbr_string), wbr("word."))
    self.assertEquals("word/{0}".format(wbr_string), wbr("word/"))
    self.assertEquals("word_{0}".format(wbr_string), wbr("word_"))

    self.assertEquals("word.{0}break".format(wbr_string), wbr("word.break"))
    self.assertEquals("word/{0}break".format(wbr_string), wbr("word/break"))
    self.assertEquals("word_{0}break".format(wbr_string), wbr("word_break"))

    # Test words with more components
    self.assertEquals("word_{0}break_{0}again".format(wbr_string),
                      wbr("word_break_again"))
    self.assertEquals("word_{0}break_{0}again_{0}emphasis".format(wbr_string),
                      wbr("word_break_again_emphasis"))

    # Words with 2 or less subcomponents are ignored for the capital letters
    self.assertEquals("word_{0}breakIgnored".format(wbr_string),
                      wbr("word_breakIgnored"))
    self.assertEquals("wordIgnored".format(wbr_string),
                      wbr("wordIgnored"))

    # Words with at least 3 sub components get word breaks before caps
    self.assertEquals("word_{0}break_{0}again{0}Capitalized".format(wbr_string),
                      wbr("word_break_againCapitalized"))
    self.assertEquals("word.{0}break.{0}again{0}Capitalized".format(wbr_string),
                      wbr("word.break.againCapitalized"))
    self.assertEquals("a.{0}b{0}C.{0}d{0}E.{0}f{0}G".format(wbr_string),
                      wbr("a.bC.dE.fG"))

    # Don't be overly aggressive with all caps
    self.assertEquals("TRANSFORM_{0}MATRIX".format(wbr_string),
                      wbr("TRANSFORM_MATRIX"))

    self.assertEquals("SCENE_{0}MODE_{0}FACE_{0}PRIORITY".format(wbr_string),
                      wbr("SCENE_MODE_FACE_PRIORITY"))

    self.assertEquals("android.{0}color{0}Correction.{0}mode is TRANSFORM_{0}MATRIX.{0}".format(wbr_string),
                      wbr("android.colorCorrection.mode is TRANSFORM_MATRIX."))

    self.assertEquals("The overrides listed for SCENE_{0}MODE_{0}FACE_{0}PRIORITY are ignored".format(wbr_string),
                      wbr("The overrides listed for SCENE_MODE_FACE_PRIORITY are ignored"));

if __name__ == '__main__':
    unittest.main()

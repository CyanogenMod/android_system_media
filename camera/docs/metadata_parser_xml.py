#!/usr/bin/python

#
# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
A parser for metadata_properties.xml can also render the resulting model
over a Mako template.

Usage:
  metadata_parser_xml.py <filename.xml>
  - outputs the resulting template to stdout

Module:
  The parser is also available as a module import (MetadataParserXml) to use
  in other modules.

Dependencies:
  BeautifulSoup - an HTML/XML parser available to download from
          http://www.crummy.com/software/BeautifulSoup/
  Mako - a template engine for Python, available to download from
     http://www.makotemplates.org/
"""

import sys

from bs4 import BeautifulSoup
from bs4 import NavigableString

from mako.template import Template

from metadata_model import *
from metadata_validate import *

class MetadataParserXml:
  """
  A class to parse any XML file that passes validation with metadata-validate.
  It builds a metadata_model.Metadata graph and then renders it over a
  Mako template.

  Attributes (Read-Only):
    soup: an instance of BeautifulSoup corresponding to the XML contents
    metadata: a constructed instance of metadata_model.Metadata
  """
  def __init__(self, file_name):
    """
    Construct a new MetadataParserXml, immediately try to parse it into a
    metadata model.

    Args:
      file_name: path to an XML file that passes metadata-validate

    Raises:
      ValueError: if the XML file failed to pass metadata_validate.py
    """
    self._soup = validate_xml(file_name)

    if self._soup is None:
      raise ValueError("%s has an invalid XML file" %(file_name))

    self._metadata = Metadata()
    self._parse()
    self._metadata.construct_graph()

  @property
  def soup(self):
    return self._soup

  @property
  def metadata(self):
    return self._metadata

  @staticmethod
  def _find_direct_strings(element):
    if element.string is not None:
      return [element.string]

    return [i for i in element.contents if isinstance(i, NavigableString)]

  @staticmethod
  def _strings_no_nl(element):
    return "".join([i.strip() for i in MetadataParserXml._find_direct_strings(element)])

  def _parse(self):

    tags = self.soup.tags
    if tags is not None:
      for tag in tags.find_all('tag'):
        self.metadata.insert_tag(tag['id'], tag.string)

    for entry in self.soup.find_all("entry"):
      d = {
         'name': fully_qualified_name(entry),
         'type': entry['type'],
         'kind': find_kind(entry),
         'type_notes': entry.attrs.get('type_notes')
      }

      d2 = self._parse_entry(entry)
      d3 = self._parse_entry_optional(entry)

      entry_dict = dict(d.items() + d2.items() + d3.items())
      self.metadata.insert_entry(entry_dict)

    entry = None

    for clone in self.soup.find_all("clone"):
      d = {
         'name': clone['entry'],
         'kind': find_kind(clone),
         'target_kind': clone['kind'],
        # no type since its the same
        # no type_notes since its the same
      }

      d2 = self._parse_entry_optional(clone)
      clone_dict = dict(d.items() + d2.items())
      self.metadata.insert_clone(clone_dict)

    self.metadata.construct_graph()

  def _parse_entry(self, entry):
    d = {}

    #
    # Enum
    #
    if entry['type'] == 'enum':

      enum_values = []
      enum_optionals = []
      enum_notes = {}
      enum_ids = {}
      for value in entry.enum.find_all('value'):

        value_body = self._strings_no_nl(value)
        enum_values.append(value_body)

        if value.attrs.get('optional', 'false') == 'true':
          enum_optionals.append(value_body)

        notes = value.find('notes')
        if notes is not None:
          enum_notes[value_body] = notes.string

        if value.attrs.get('id') is not None:
          enum_ids[value_body] = value['id']

      d['enum_values'] = enum_values
      d['enum_optionals'] = enum_optionals
      d['enum_notes'] = enum_notes
      d['enum_ids'] = enum_ids

    #
    # Container (Array/Tuple)
    #
    if entry.attrs.get('container') is not None:
      container_name = entry['container']

      array = entry.find('array')
      if array is not None:
        array_sizes = []
        for size in array.find_all('size'):
          array_sizes.append(size.string)
        d['container_sizes'] = array_sizes

      tupl = entry.find('tuple')
      if tupl is not None:
        tupl_values = []
        for val in tupl.find_all('value'):
          tupl_values.append(val.name)
        d['tuple_values'] = tupl_values
        d['container_sizes'] = len(tupl_values)

      d['container'] = container_name

    return d

  def _parse_entry_optional(self, entry):
    d = {}

    optional_elements = ['description', 'range', 'units', 'notes']
    for i in optional_elements:
      prop = find_child_tag(entry, i)

      if prop is not None:
        d[i] = prop.string

    tag_ids = []
    for tag in entry.find_all('tag'):
      tag_ids.append(tag['id'])

    d['tag_ids'] = tag_ids

    return d

  def render(self, template, output_name=None):
    """
    Render the metadata model using a Mako template as the view.

    Args:
      template: path to a Mako template file
      output_name: path to the output file, or None to use stdout
    """
    tpl = Template(filename=template)
    tpl_data = tpl.render(metadata=self.metadata)

    if output_name is None:
      print tpl_data
    else:
      file(output_name, "w").write(tpl_data)

#####################
#####################

if __name__ == "__main__":
  if len(sys.argv) <= 1:
    print >> sys.stderr, "Usage: %s <filename.xml>" % (sys.argv[0])
    sys.exit(0)

  file_name = sys.argv[1]
  parser = MetadataParserXml(file_name)
  parser.render("metadata_template.mako")

  sys.exit(0)

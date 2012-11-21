# Camera Metadata XML
## Introduction
This is a set of scripts to manipulate the camera metadata in an XML form.

## Generated Files
Many files can be generated from XML, such as the documentation (html/pdf),
C code, Java code, and even XML itself (as a sanity check).

## Dependencies:
sudo apt-get install python-mako # mako templates. needed to do file generation
sudo apt-get install python-bs4  # beautiful soup. needed to parse the xml
sudo apt-get install tidy        # tidy, used to clean up xml/html
sudo apt-get install xmllint     # xmllint, used to validate XML against XSD

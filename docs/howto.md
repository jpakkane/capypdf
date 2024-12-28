# How to do various things with CapyPDF

The example code used here is Python as it is the most compact and
readable, but the same can be done with any language.

## General design

The APIs in CapyPDF are designed to be transactional. That is, you
first create an object for doing something, then call methods on it to
change its state. Eventually you commit it to the document with a
suitable method.

```python
thing = SomeThing()
thing.set_width(300)
thing.set_height(100)
gen.add_thing(thing)
```

Any changes made to the `thing` object are not used for generating the
final PDF. After calling `add_thing` the object's state is stored
inside the gneerator. Changing values in the `thing` object do not
affect the object that was committed.

In more technical terms the data model is append only.

## Strings in the C API

All functions that take strings also take a length argument like so:

```c
void capy_something(const char *str, int32_t strsize);
```

This `strsize` argument can either be set to the length of the string
array (in bytes) or the value `-1`. The latter means that the string
pointed to is zero terminated in the usual C way.

Strings may _not_ have embedded zero characters.

Filenames do not take a length argument, they are always zero
terminated.

## Creating PDF/A document

First you need to specify the PDF/A type.

```python
props = capypdf.DocumentProperties()
props.set_pdfa(capypdf.PdfAType.A3u)
```

PDF/A documents require a output ICC profile.

```python
props.set_device_colorspace(capypdf.DeviceColorspace.RGB,
                            'path/to/srgb.icc')
props.set_output_intent('sRGB <details>')
```

In the event that you need a custom rdf metadata entry, you need to
set it as well.

```python
props.set_metadata_xml(your_rdf_xml)
```

If you don't specify this, CapyPDF will autogenerate one for
you. Note, though, that if you _do_ specify it, then CapyPDF writes it
to the file unaltered. Thus it must have _all_ the information
(including the PDF/A values) and it is the responsibility of the
caller to ensure the content is valid.

## Convert images to output colorspace

Suppose you want to create a CMYK document, but some of your input
images are in RGB. Thus they need to be converted. This is one way of
getting it done:

```python
image = gen.load_image(image_path)
iprops = capypdf.ImagePdfProperties()
if image.get_colorspace() == capypdf.ImageColorspace.RGB:
    image = gen.convert_image(image,
                              capypdf.ImageColorspace.CMYK,
                              capypdf.RenderingIntent.RelativeColorimetric)
image_id = gen.add_image(image, iprops)
```

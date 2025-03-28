#!/usr/bin/python3
# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-
#
#   Copyright (C) 2019 Sean Davis <bluesabre@xfce.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
#
#   Authors: Sean Davis <bluesabre@xfce.org>

import argparse
import os
import sys
import xml.etree.ElementTree as ET
import warnings

from collections import OrderedDict

from locale import gettext as _

import gi
gi.require_version('GLib', '2.0')
gi.require_version('GObject', '2.0')
gi.require_version('Gdk', '3.0')
gi.require_version('Gtk', '3.0')
gi.require_version('Libxfce4util', '1.0')
gi.require_version('Xfconf', '0')
from gi.repository import GLib, GObject, Gdk, Gtk, Libxfce4util, Xfconf

warnings.filterwarnings("ignore")


class XfconfSettings(object):

    def __init__(self, channel_obj, prop_base):
        self.channel = channel_obj
        self.prefix = prop_base

    def _check_property_name(self, name):
        if name.startswith('/'):
            return name
        else:
            return '{0}/{1}'.format(self.prefix, name)

    def restore_defaults(self):
        self.channel.reset_property(self.prefix, True)

    def get_boolean(self, prop):
        g_value = GObject.Value(GObject.TYPE_BOOLEAN)
        self.channel.get_property(self._check_property_name(prop),
                                  g_value)
        if g_value.get_boolean() in ["true", True]:
            return True
        return False

    def set_boolean(self, prop, value):
        g_value = GObject.Value(GObject.TYPE_BOOLEAN)
        g_value.set_boolean(value)
        self.channel.set_property(self._check_property_name(prop),
                                  g_value)

    def get_double(self, prop):
        g_value = GObject.Value(GObject.TYPE_DOUBLE)
        self.channel.get_property(self._check_property_name(prop),
                                  g_value)
        return g_value.get_double()

    def set_double(self, prop, value):
        g_value = GObject.Value(GObject.TYPE_DOUBLE)
        g_value.set_double(float(value))
        self.channel.set_property(self._check_property_name(prop),
                                  g_value)

    def get_int(self, prop):
        g_value = GObject.Value(GObject.TYPE_INT)
        self.channel.get_property(self._check_property_name(prop),
                                  g_value)
        return g_value.get_int()

    def set_int(self, prop, value):
        g_value = GObject.Value(GObject.TYPE_INT)
        g_value.set_int(int(value))
        self.channel.set_property(self._check_property_name(prop),
                                  g_value)

    def get_string(self, prop):
        g_value = GObject.Value(GObject.TYPE_STRING)
        self.channel.get_property(self._check_property_name(prop),
                                  g_value)
        return g_value.get_string()

    def set_string(self, prop, value):
        g_value = GObject.Value(GObject.TYPE_STRING)
        g_value.set_string(value)
        self.channel.set_property(self._check_property_name(prop),
                                  g_value)

    def check_property(self, prop):
        return self.channel.has_property(self._check_property_name(prop))


class ScreensaverSettings:

    def __init__(self):
        self.name = None
        self.label = None
        self.arguments = None
        self.options = OrderedDict()
        self.description = None
        self.video = None
        self.filename = None
        self.valid = False
        self.configurable = False

    def to_dict(self):
        parsed = {}
        parsed['name'] = self.name
        parsed['label'] = self.label
        parsed['arguments'] = self.arguments
        parsed['options'] = self.options
        parsed['description'] = self.description
        parsed['video'] = self.video
        for key in parsed:
            if parsed[key] is None:
                parsed[key] = ""
        return parsed


class DesktopScreensaverSettings(ScreensaverSettings):

    def __init__(self, name):
        ScreensaverSettings.__init__(self)
        self.name = name

    def load_from_file(self, filename):
        if not os.path.exists(filename):
            return False

        self.filename = filename

        keyfile = GLib.KeyFile.new()
        if not keyfile.load_from_file(filename, GLib.KeyFileFlags.NONE):
            return False

        self.valid = True

        label = keyfile.get_locale_string("Desktop Entry", "Name", None)
        comment = keyfile.get_locale_string("Desktop Entry", "Comment", None)
        command = keyfile.get_string("Desktop Entry", "Exec")
        if self.name in ["xfce-floaters", "xfce-personal-slideshow", "xfce-popsquares"]:
            options = self.parse_internal()

        self.label = label
        self.arguments = command.split(" ")[1:]
        self.options = options
        self.description = "%s\n\n%s" % (comment, _("Part of Xfce Screensaver"))

        if self.options:
            self.configurable = True

        return True

    def parse_internal(self):
        options = OrderedDict()
        if self.name == "xfce-floaters":
            options["number-of-images"] = {'id': 'number-of-images', 'type': 'spinbutton',
                                           'label': _("Max number of images"), 'argument': '-n %',
                                           'default': 5, 'low': 1, 'high': 1000}
            options["show-paths"] = {'id': 'show-paths', 'type': 'checkbox',
                                     'label': _("Show paths"), 'argument': '-p'}
            options["do-rotations"] = {'id': 'do-rotations', 'type': 'checkbox',
                                       'label': _("Do rotations"), 'argument': '-r'}
            options["print-stats"] = {'id': 'print-stats', 'type': 'checkbox',
                                      'label': _("Print stats"), 'argument': '-s'}
        elif self.name == "xfce-personal-slideshow":
            pics = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_PICTURES)
            options["location"] = {'id': 'location', 'type': 'folder', 'label': _("Location"),
                                   'argument': '--location=%', 'default': pics if pics != None else '~'}
            options["background-color"] = {'id': 'background-color', 'type': 'color',
                                           'label': _("Background color"), 'argument': '--background-color=%', 'default': '#000000'}
            options["sort-images"] = {'id': 'sort-images', 'type': 'checkbox',
                                      'label': _("Do not randomize images"), 'argument': '--sort-images'}
            options["no-stretch"] = {'id': 'no-stretch', 'type': 'checkbox',
                                     'label': _("Do not stretch images"), 'argument': '--no-stretch'}
            options["no-crop"] = {'id': 'no-crop', 'type': 'checkbox',
                                  'label': _("Do not crop images"), 'argument': '--no-crop', 'default': True}
        return options


class XmlScreensaverSettings(ScreensaverSettings):

    def __init__(self, name):
        ScreensaverSettings.__init__(self)
        self.name = name

    def load_from_file(self, filename):
        if not os.path.exists(filename):
            return False

        self.filename = filename
        element = ET.parse(filename).getroot()

        root = self.parse_element(element)
        self.name = root["attributes"]["name"]
        self.label = root["attributes"]["label"]
        self.arguments = []

        for item in self.get_data_elements(element):
            parsed = self.parse_element(item)
            element_type = parsed["type"]
            parsed.pop("type", None)
            if element_type == "property":
                if parsed["id"] == "command":
                    self.arguments.append(parsed["attributes"]["argument"])
                elif parsed["id"] == "description":
                    self.description = parsed["text"]
                elif parsed["id"] == "video":
                    self.video = parsed["attributes"]["href"]
            elif element_type == "option":
                if "options" in parsed.keys():
                    parsed["attributes"]["options"] = parsed["options"]
                self.options[parsed["id"]] = parsed["attributes"]

        if self.options:
            self.configurable = True

        return True

    def get_data_elements(self, element):
        results = []
        for child in list(element):
            if child.tag == "vgroup" or child.tag == "hgroup":
                results = results + self.get_data_elements(child)
            elif child.tag.startswith("xscreensaver"):
                pass
            else:
                if child.tag.startswith("_"):
                    child.tag = child.tag[1:]
                    if child.text is not None:
                        child.text = _(child.text)
                results.append(child)
        return results

    def parse_element_attributes(self, element):
        attributes = {
            "id": "",
            "type": "",
            "label": "",
        }

        for key in element.attrib:
            value = element.attrib[key]
            if key.startswith("_"):
                key = key[1:]
                if value is not None:
                    value = _(value)
            if key in ["arg", "arg-set", "arg-unset"]:
                key = "argument"
            if "#" in value:
                tmp = value.split(" ")
                parts = []
                for piece in tmp:
                    if piece.startswith("#"):
                        piece = "'%s'" % piece
                    parts.append(piece)
                value = " ".join(parts)
            attributes[key] = value

        return attributes

    def parse_element_id(self, element, attributes):
        if attributes["id"]:
            element_id = attributes["id"]
            element_type = "option"
        else:
            element_id = element.tag
            element_type = "property"
        return (element_id, element_type)

    def parse_element(self, element):
        text = None
        options = None

        attributes = self.parse_element_attributes(element)
        element_id, element_type = self.parse_element_id(element, attributes)

        if element.tag == "select":
            attributes["type"] = "select"
            options = []
            for child in element.findall("option"):
                options.append(self.parse_element(child))
        elif element.tag == "option":
            if not attributes["id"] and attributes["label"]:
                element_id = attributes["label"].replace(" ", "-").lower()
            out = {"id": element_id, "label": attributes["label"]}
            if "argument" in attributes.keys():
                out["argument"] = attributes["argument"]
            return out

        if attributes["type"] in ["slider", "spinbutton"]:
            for attrib in ["high", "low", "default"]:
                if attrib in attributes.keys():
                    attributes[attrib] = float(attributes[attrib])

        if not attributes["type"]:
            if "argument" in attributes.keys():
                attributes["type"] = "checkbox"
            if attributes["id"] == "file":
                attributes["type"] = "file"

        if attributes["label"] == "":
            attributes["label"] = attributes["id"].capitalize()

        out = {"id": element_id, "type": element_type, "attributes": attributes}

        if element.text:
            text = str(element.text).strip()
            if text:
                out["text"] = text

        if options:
            out["options"] = options

        return out


class ConfigurationWindow(Gtk.Window):

    def __init__(self, parsed):
        Gtk.Window.__init__(self, title=parsed["label"])

        self.set_modal(True)
        self.set_border_width(6)
        self.set_default_size(400, -1)
        self.set_icon_name("preferences-desktop-screensaver")

        self.inner_margin = 12
        self.screensaver_args = parsed["arguments"]

        property_base = '/screensavers/{0}'.format(parsed['name'])
        channel = Xfconf.Channel.get('xfce4-screensaver')
        if channel is not None:
            self.xfconf_settings = XfconfSettings(channel,
                                                  property_base)
        else:
            print("Unable to access channel", file=sys.stderr)
            sys.exit(1)

        self.notebook = Gtk.Notebook.new()
        self.grid = Gtk.Grid.new()
        self.widgets = dict()

        self.lookup_table = OrderedDict()
        self.defaults = {}
        self.settings = OrderedDict()

        self.add(self.notebook)
        self.setup_grid()
        self.setup_notebook(parsed)

        row = 0

        for opt_key in parsed["options"]:
            opt = parsed["options"][opt_key]

            label = Gtk.Label.new(opt["label"])
            label.set_halign(Gtk.Align.START)
            self.grid.attach(label, 0, row, 1, 1)

            widget = self.get_option_widget(opt)
            widget.set_hexpand(True)
            self.grid.attach(widget, 1, row, 1, 1)

            self.widgets[opt_key] = (widget, opt["type"])

            row += 1

        widget = Gtk.Button.new_with_label(_("Restore Defaults"))
        widget.set_margin_top(18)
        widget.connect("clicked", self.on_restore_clicked)
        self.grid.attach(widget, 0, row, 2, 1)

    def setup_grid(self):
        self.grid.set_column_spacing(12)
        self.grid.set_row_spacing(6)
        self.grid.set_margin_top(self.inner_margin)
        self.grid.set_margin_bottom(self.inner_margin)
        self.grid.set_margin_start(self.inner_margin)
        self.grid.set_margin_end(self.inner_margin)

        label = Gtk.Label.new(_("Preferences"))
        self.notebook.append_page(self.grid, label)

    def setup_notebook(self, parsed):
        if parsed["description"] != "":
            vbox = Gtk.Box.new(Gtk.Orientation.VERTICAL, 18)
            label = Gtk.Label.new(_("About"))
            self.notebook.append_page(vbox, label)
            vbox.set_margin_top(self.inner_margin)
            vbox.set_margin_bottom(self.inner_margin)
            vbox.set_margin_start(self.inner_margin)
            vbox.set_margin_end(self.inner_margin)

            label = Gtk.Label.new(parsed["description"])
            label.set_valign(Gtk.Align.START)
            vbox.pack_start(label, True, True, 0)

            if parsed["video"] != "":
                button = Gtk.LinkButton.new_with_label(
                    parsed["video"], _("Video"))
                vbox.pack_start(button, False, False, 0)
        else:
            self.notebook.set_show_tabs(False)

    def get_option_widget(self, opt):
        if "argument" in opt.keys():
            self.lookup_table[opt["id"]] = opt["argument"]

        if opt["type"] == "checkbox":
            widget = Gtk.CheckButton.new_with_label("")
            try:
                default = opt["default"]
            except:
                default = False

            if self.xfconf_settings.check_property(opt['id']):
                active = self.xfconf_settings.get_boolean(opt['id'])
                self.defaults[opt['id']] = default
            else:
                active = self.get_boolean(opt['id'], default)

            widget.connect("toggled", self.on_checkbutton_toggle, opt["id"])
            widget.set_active(active)
        elif opt["type"] == "file":
            widget = Gtk.FileChooserButton(
                title=opt["label"], action=Gtk.FileChooserAction.OPEN)
            filename = self.get_string(opt["id"], "")
            widget.set_current_folder("~")
            widget.connect("selection-changed",
                           self.on_file_changed, opt["id"])
            widget.set_filename(filename)
        elif opt["type"] == "folder":
            widget = Gtk.FileChooserButton(
                title=opt["label"], action=Gtk.FileChooserAction.SELECT_FOLDER)
            folder = self.get_string(opt["id"], opt["default"])
            widget.set_current_folder(folder)
            widget.connect("selection-changed",
                           self.on_file_changed, opt["id"])
            widget.set_filename(folder)
        elif opt["type"] == "select":
            self.lookup_table[opt["id"]] = {}
            widget = Gtk.ComboBoxText.new()
            for option in opt["options"]:
                opt_id = get_key(option, "id", "")
                opt_label = get_key(option, "label", "")
                opt_argument = get_key(option, "argument", "")
                widget.append(opt_id, opt_label)
                self.lookup_table[opt["id"]][opt_id] = opt_argument

            if self.xfconf_settings.check_property(opt['id']):
                current = self.xfconf_settings.get_string(opt['id'])
                self.defaults[opt['id']] = ''
            else:
                current = self.get_string(opt['id'], '')

            widget.connect("changed", self.on_select_changed, opt["id"])
            widget.set_active_id(current)
        elif opt["type"] == "slider":
            prefs = get_slider_prefs([opt["default"], opt["low"], opt["high"]])
            adj = Gtk.Adjustment(value=opt["default"], lower=opt["low"], upper=opt["high"],
                                 step_increment=prefs['step'], page_increment=10, page_size=0)
            widget = Gtk.Scale(
                orientation=Gtk.Orientation.HORIZONTAL, adjustment=adj)
            widget.add_mark(
                opt["low"], Gtk.PositionType.BOTTOM, opt["low-label"])
            widget.add_mark(
                opt["high"], Gtk.PositionType.BOTTOM, opt["high-label"])
            widget.set_digits(prefs['digits'])

            if self.xfconf_settings.check_property(opt['id']):
                value = self.xfconf_settings.get_double(opt['id'])
                self.defaults[opt['id']] = opt['default']
            else:
                value = self.get_double(opt['id'], opt['default'])

            widget.connect("value-changed", self.on_double_changed, opt["id"])
            adj.set_value(value)
        elif opt["type"] == "spinbutton":
            widget = Gtk.SpinButton.new_with_range(opt["low"], opt["high"], 1)

            if self.xfconf_settings.check_property(opt['id']):
                value = self.xfconf_settings.get_int(opt['id'])
                self.defaults[opt['id']] = opt['default']
            else:
                value = self.get_int(opt['id'], opt['default'])

            widget.connect("value-changed", self.on_int_changed, opt["id"])
            widget.set_value(value)
        elif opt["type"] == "color":
            widget = Gtk.ColorButton.new()
            color = self.get_string(opt["id"], "#000000")
            widget.connect("color-set", self.on_color_changed, opt["id"])
            widget.set_rgba(hex_to_rgba(color))
        else:
            print("Unrecognized type: %s" % opt["type"], file=sys.stderr)
            sys.exit(1)
        return widget

    def get_boolean(self, option_id, default):
        if self.xfconf_settings.check_property(option_id):
            value = self.xfconf_settings.get_boolean(option_id)
        else:
            value = default
        self.defaults[option_id] = default
        self.set_boolean(option_id, value, False)
        return value

    def set_boolean(self, option_id, value, store=True):
        self.settings[option_id] = value
        if store:
            self.xfconf_settings.set_boolean(option_id, value)

    def get_double(self, option_id, default):
        if self.xfconf_settings.check_property(option_id):
            value = self.xfconf_settings.get_double(option_id)
        else:
            value = default
        self.defaults[option_id] = default
        self.set_double(option_id, value, False)
        return value

    def set_double(self, option_id, value, store=True):
        self.settings[option_id] = value
        if store:
            self.xfconf_settings.set_double(option_id, value)

    def get_int(self, option_id, default):
        if self.xfconf_settings.check_property(option_id):
            value = self.xfconf_settings.get_int(option_id)
        else:
            value = default
        self.defaults[option_id] = default
        self.set_int(option_id, value, False)
        return value

    def set_int(self, option_id, value, store=True):
        self.settings[option_id] = value
        if store:
            self.xfconf_settings.set_int(option_id, value)

    def get_string(self, option_id, default):
        if self.xfconf_settings.check_property(option_id):
            value = self.xfconf_settings.get_string(option_id)
        else:
            value = default
        self.defaults[option_id] = default
        self.set_string(option_id, value, False)
        return value

    def set_string(self, option_id, value, store=True):
        self.settings[option_id] = value
        if store:
            self.xfconf_settings.set_string(option_id, value)

    def on_checkbutton_toggle(self, button, option_id):
        self.set_boolean(option_id, button.get_active())
        self.write_arguments()

    def on_file_changed(self, button, option_id):
        filename = button.get_filename()
        self.set_string(option_id, filename)
        self.write_arguments()

    def on_select_changed(self, combobox, option_id):
        active = combobox.get_active_id()
        self.set_string(option_id, active)
        self.write_arguments()

    def on_double_changed(self, slider, option_id):
        value = slider.get_value()
        self.set_double(option_id, value)
        self.write_arguments()

    def on_int_changed(self, slider, option_id):
        value = int(slider.get_value())
        self.set_int(option_id, value)
        self.write_arguments()

    def on_color_changed(self, widget, option_id):
        rgba = widget.get_rgba()
        self.set_string(option_id, rgba_to_hex(rgba))
        self.write_arguments()

    def on_restore_clicked(self, button):
        for widget_id in self.widgets:
            widget, widget_type = self.widgets[widget_id]
            if widget_type == "checkbox":
                widget.set_active(self.defaults[widget_id])
            elif widget_type == "file":
                widget.set_filename(self.defaults[widget_id])
            elif widget_type == "folder":
                widget.set_filename(self.defaults[widget_id])
                widget.set_current_folder(self.defaults[widget_id])
            elif widget_type == "select":
                widget.set_active(0)
            elif widget_type == "slider":
                widget.set_value(self.defaults[widget_id])
            elif widget_type == "spinbutton":
                widget.set_value(self.defaults[widget_id])
            elif widget_type == "color":
                color = self.defaults[widget_id]
                widget.set_rgba(hex_to_rgba(color))
                self.set_string(widget_id, color)
        self.xfconf_settings.restore_defaults()

    def write_arguments(self):
        arguments = []
        for setting in self.settings:
            value = self.settings[setting]
            if value not in ("", False):
                if setting in self.lookup_table:
                    argument = self.lookup_table[setting]
                    if isinstance(argument, dict):
                        argument = argument[value]
                    elif "%" in argument:
                        try:
                            value = int(value)
                        except:
                            pass
                        argument = argument.replace("%", str(value))
                    arguments.append(GLib.shell_quote(argument))
        value = " ".join(arguments)
        self.xfconf_settings.set_string("arguments", value)


def get_slider_prefs(options):
    digits = 0
    for val in options:
        parts = str(val).split(".")
        if len(parts) < 2:
            digits = 0
        else:
            val = parts[-1]
            if val != "0":
                val = val.rstrip("0")
                digits = max(digits, len(val))
    if digits == 0:
        return {'digits': 0, 'step': 1}
    return {'digits': digits, 'step': 10 ^ (-1 * digits)}


def rgba_to_hex(rgba):
    value = '#%02x%02x%02x' % (int(round(
        rgba.red * 255, 0)), int(round(rgba.green * 255, 0)), int(round(rgba.blue * 255, 0)))
    value = value.upper()
    return value


def hex_to_rgba(value):
    rgba = Gdk.RGBA()
    rgba.parse(value)
    return rgba


def get_filename(theme):
    user_data = [GLib.get_user_data_dir()]
    system_data = GLib.get_system_data_dirs()

    data_dirs = user_data + [i for i in system_data if i not in user_data]

    for config_file in ["%s/xscreensaver/config/%s.xml", "%s/applications/screensavers/%s.desktop"]:
        for data_dir in data_dirs:
            cfg = config_file % (data_dir, theme)
            if os.path.exists(cfg):
                return cfg

    return None


def get_key(dct, keyname, default=""):
    if keyname in dct.keys():
        return dct[keyname]
    return default


def configure(parsed):
    try:
        win = ConfigurationWindow(parsed)
        win.connect("destroy", Gtk.main_quit)
        win.set_position(Gtk.WindowPosition.MOUSE)
        win.show_all()
        Gtk.main()
    except KeyboardInterrupt:
        pass


def show_fatal(primary, secondary):
    if not graphical:
        print("%s: %s" % (primary, secondary), file=sys.stderr)
        sys.exit(1)
    try:
        dlg = Gtk.MessageDialog(None, 0, Gtk.MessageType.ERROR,
                                Gtk.ButtonsType.OK, primary)
        dlg.format_secondary_text(secondary)
        dlg.set_position(Gtk.WindowPosition.MOUSE)
        dlg.run()
    except KeyboardInterrupt:
        pass
    sys.exit(1)


if __name__ == "__main__":
    Libxfce4util.textdomain('xfce4-screensaver',
                            os.path.join(os.path.dirname(os.path.realpath(__file__)), '../share/locale'),
                            'UTF-8')

    parser = argparse.ArgumentParser(
        description=_("Configure an individual screensaver"))
    parser.add_argument('screensaver', metavar='N', type=str, nargs='?',
                        help=_("screensaver name to configure"))
    parser.add_argument('--check', action='store_true',
                        help=_("check if screensaver is configurable"))
    args = parser.parse_args()

    graphical = not args.check
    primary = _("Unable to configure screensaver")

    saver = args.screensaver
    if saver is None:
        show_fatal(primary, _("Screensaver required."))
        sys.exit(1)

    if saver.startswith("screensavers-"):
        saver = saver[13:]

    fname = get_filename(saver)
    if fname is None:
        show_fatal(primary, _("File not found: %s") % saver)
        sys.exit(1)

    if fname.endswith(".xml"):
        obj = XmlScreensaverSettings(saver)
    elif fname.endswith(".desktop"):
        obj = DesktopScreensaverSettings(saver)
    else:
        show_fatal(primary, _("Unrecognized file type: %s") % fname)
        sys.exit(1)

    if not obj.load_from_file(fname):
        show_fatal(primary, _("Failed to process file: %s") % fname)
        sys.exit(1)

    if args.check:
        if obj.configurable:
            print(_("Screensaver %s is configurable.") % saver)
        else:
            print(_("Screensaver %s is not configurable.") % saver)
        sys.exit(0)

    if not obj.configurable:
        show_fatal(primary, _("Screensaver %s has no configuration options.") % saver)

    if not Xfconf.init():
        print("Failed to initialize Xfconf", file=sys.stderr)
        sys.exit(1)

    configure(obj.to_dict())

    Xfconf.shutdown()

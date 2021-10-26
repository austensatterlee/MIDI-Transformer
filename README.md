## Download
Platform | Link
---------|-------
Windows x64 | <a href="https://static.kvraudio.com/files/4296/midi-transformer-0_9_1.zip">VST3</a>

# MIDI-Transformer
A free VST for transforming and bending MIDI inputs

<p align="center">
<img src="https://raw.github.com/austensatterlee/MIDI-Transformer/master/screenshots/quadratic_curves.png"
width=800>
</p>

## Explanation

To use the plugin, place it somewhere in your VST chain and select an input source from the top-left dropdown, as well as an output destination from the top-right dropdown.

During playback, the plugin works as follows:

  - **Reads** incoming MIDI values from the selected input source.
  - **Transforms** those values using the specified transformation curve.
  - **Outputs** the transformed values according to the selected output destination.

For example, if the input source was set to CC2 and the output source was set to CC3, the plugin would read all incoming CC2 values, transform them, and then output the transformed values as CC3 messages.

The plugin's GUI will show a vertical line along the curve to indicate the last input value that was captured, and how it was transformed.

## Editing the curve

  - **Move** a node by clicking and dragging.
  - **Add** a node by double clicking anywhere along the curve.
  - **Delete** a node by right clicking on it.
  - **Change** a node's curve type by double clicking on it. This will cycle between the three curve types:
      - *Linear* (default).
      - *Quadratic*
      - *Cubic*

When a node's curve type is set to Quadratic, it will have one attached handle. When set to Cubic, it will have two attached handles. Moving these handles around allows you to modify the shape of the curve more precisely.

## Saving

The plugin's state will be saved and managed automatically by the DAW. There is currently no built-in preset manager.

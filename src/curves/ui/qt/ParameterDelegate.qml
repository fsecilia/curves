import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// This component defines *one row* in the Parameter ListView.
// It's instantiated by the ListView for each item in the
// CurveParameterModel.
//
// The 'model' property is automatically provided by the ListView.
RowLayout {
    width: parent.width // Fill the width of the ListView
    spacing: 5

    Label {
        // 'model.label' reads the 'LabelRole' from the C++ model
        text: model.label + ":"
        color: "white"
        Layout.fillWidth: true
        Layout.preferredWidth: 80 // Give the label a weight
    }

    // Here you could use a 'Loader' or 'StackLayout' to dynamically
    // show a TextField, CheckBox, etc., based on 'model.type'.
    // For now, we'll just show a TextField for everything.
    TextField {
        // 'model.value' reads the 'ValueRole'
        text: model.value

        // This is where two-way binding would happen.
        // You would need a C++ function to update the model.
        // onEditingFinished: {
        //    editorPresenter.setParameterValue(model.index, text)
        // }

        Layout.fillWidth: true
        Layout.preferredWidth: 120 // Give the textfield a weight

        // Use a validator if the model provides one
        // validator: (model.type === "double") ? DoubleValidator {} : null
    }
}

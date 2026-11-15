#!/usr/bin/env python3
import io, sys
files = [
    'test/unit/Button_test.cpp','test/unit/ControlTemplate_test.cpp',
    'test/unit/ConverterSurface_test.cpp','test/unit/ResourceResolution_test.cpp',
    'test/unit/VmAgnosticInjection_test.cpp','test/unit/TextBox_test.cpp',
    'test/unit/PasswordGroupStatus_test.cpp','test/unit/TreeView_s552_test.cpp',
    'test/unit/DateCalendar_s553_test.cpp','test/unit/ContainerMenu_s11e_test.cpp',
    'test/unit/ViewerHitTest_s595a_test.cpp','test/unit/ViewerSeedBrush_s621_test.cpp',
    'test/unit/ViewerLoadFile_test.cpp','test/unit/HexBrushId_s643a_test.cpp',
    'test/unit/HexBrushId_s643b_test.cpp','test/unit/TemplateBinding_test.cpp',
    'test/unit/ViewerGrowth_s486_test.cpp','test/unit/ViewerGrowth_s495_test.cpp',
    'test/unit/PropertyElement_s452_test.cpp','test/unit/ValueTypeAware_s600_test.cpp',
    'test/unit/StructuredError_s604_test.cpp','test/unit/GeometricValue_s601_test.cpp',
]
for f in files:
    try:
        with io.open(f, 'r+', encoding='utf-8', newline='') as h:
            pass
        print('WRITABLE', f)
    except Exception:
        print('LOCKED  ', f)

#!/usr/bin/env python3
import io
files = [
    'test/unit/ConverterSurface_test.cpp','test/unit/ResourceResolution_test.cpp',
    'test/unit/VmAgnosticInjection_test.cpp','test/unit/TextBox_test.cpp',
    'test/unit/PropertyElement_s452_test.cpp','test/unit/StyleApply_test.cpp',
    'test/unit/Button_test.cpp','test/unit/ControlTemplate_test.cpp',
    'test/unit/HexBrushId_s643a_test.cpp','test/unit/HexBrushId_s643b_test.cpp',
    'test/unit/TemplateBinding_test.cpp','test/unit/ViewerHitTest_s595a_test.cpp',
    'test/unit/ViewerSeedBrush_s621_test.cpp','test/unit/ViewerLoadFile_test.cpp',
    'test/unit/ValueTypeAware_s600_test.cpp','test/unit/StructuredError_s604_test.cpp',
    'test/unit/GeometricValue_s601_test.cpp',
]
for f in files:
    try:
        with io.open(f, 'r+', encoding='utf-8', newline='') as h:
            h.seek(0, 2)
        print('WRITABLE', f)
    except Exception as e:
        print('LOCKED  ', f)

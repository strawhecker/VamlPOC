# .NET 10.0 Upgrade Plan

## Execution Steps

Execute steps below sequentially one by one in the order they are listed.

1. Validate that a .NET 10.0 SDK required for this upgrade is installed on the machine and if not, help to get it installed.
2. Ensure that the SDK version specified in global.json files is compatible with the .NET 10.0 upgrade.
3. Upgrade Enterprise\CardiMapper.Interfaces\CardiMapper.Interfaces.csproj
4. Upgrade Enterprise\CardiMapper.Infrastructure\CardiMapper.Infrastructure.csproj
5. Upgrade Enterprise\CardiMapper.Resources\CardiMapper.Resources.csproj
6. Upgrade Toolkit\CardiMapper.Toolkit\CardiMapper.Toolkit.csproj
7. Upgrade Domain\CardiMapper.CatheterCreator.Domain\CardiMapper.CatheterCreator.Domain.csproj
8. Upgrade Domain\Cardimapper.Piu.Domain\Cardimapper.Piu.Domain.csproj
9. Upgrade Modules\CardiMapper.Anatomy.Module\CardiMapper.Anatomy.Module.csproj
10. Upgrade Domain\CardiMapper.Render.Domain\CardiMapper.Render.Domain.csproj
11. Upgrade Modules\CardiMapper.Notes.Module\CardiMapper.Notes.Module.csproj
12. Upgrade Domain\CardiMapper.Workflow.Domain\CardiMapper.Workflow.Domain.csproj
13. Upgrade Modules\CardiMapper.Render.Module\CardiMapper.Render.Module.csproj
14. Upgrade Modules\CardiMapper.Signals.Module\CardiMapper.Signals.Module.csproj
15. Upgrade Modules\CardiMapper.Chalkboard.Module\CardiMapper.Chalkboard.Module.csproj
16. Upgrade Modules\CardiMapper.QuickStatus.Module\CardiMapper.QuickStatus.Module.csproj
17. Upgrade Domain\CardiMapper.PatientCache.Domain\CardiMapper.PatientCache.Domain.csproj
18. Upgrade Modules\CardiMapper.Status.Module.Infrastructure\CardiMapper.Status.Module.Infrastructure.csproj
19. Upgrade Modules\CardiMapper.Status.Module\CardiMapper.Status.Module.csproj
20. Upgrade Modules\CardiMapper.Workflow.Module\CardiMapper.Workflow.Module.csproj
21. Upgrade Modules\CardiMapper.CatheterCreator.Module\CardiMapper.CatheterCreator.Module.csproj
22. Upgrade Modules\CardiMapper.Peripherals.Module\CardiMapper.Peripherals.Module.csproj
23. Upgrade Modules\CardiMapper.Log.Module\CardiMapper.Log.Module.csproj
24. Upgrade Modules\CardiMapper.Magnetics.Module\CardiMapper.Magnetics.Module.csproj
25. Upgrade Modules\CardiMapper.Status.Front.Panel.Module\CardiMapper.Status.Front.Panel.Module.csproj
26. Upgrade Modules\CardiMapper.Status.Collection.Module\CardiMapper.Status.Collection.Module.csproj
27. Upgrade Modules\CardiMapper.Status.Board.Module\CardiMapper.Status.Board.Module.csproj
28. Upgrade 1_CardiMapper.App\1_CardiMapper.App.csproj
29. Upgrade 1_CardiMapper.App.Tests\1_CardiMapper.App.Tests.csproj
30. Upgrade Modules\CardiMapper.QuickStatus.Module.Test\CardiMapper.QuickStatus.Module.Test.csproj
31. Upgrade Modules\CardiMapper.Signals.Module.Test\CardiMapper.Signals.Module.Test.csproj
32. Upgrade Domain\Cardimapper.Piu.Domain.Tests\Cardimapper.Piu.Domain.Tests.csproj
33. Upgrade Domain\CardiMapper.Workflow.Domain.Tests\CardiMapper.Workflow.Domain.Tests.csproj
34. Upgrade Domain\CardiMapper.Render.Domain.Tests\CardiMapper.Render.Domain.Tests.csproj
35. Upgrade Domain\CardiMapper.PatientCache.Domain.Tests\CardiMapper.PatientCache.Domain.Tests.csproj
36. Upgrade Domain\CardiMapper.CatheterCreator.Domain.Tests\CardiMapper.CatheterCreator.Domain.Tests.csproj
37. Run unit tests to validate upgrade in the projects listed below:
    - 1_CardiMapper.App.Tests\1_CardiMapper.App.Tests.csproj
    - Modules\CardiMapper.QuickStatus.Module.Test\CardiMapper.QuickStatus.Module.Test.csproj
    - Modules\CardiMapper.Signals.Module.Test\CardiMapper.Signals.Module.Test.csproj
    - Domain\Cardimapper.Piu.Domain.Tests\Cardimapper.Piu.Domain.Tests.csproj
    - Domain\CardiMapper.Workflow.Domain.Tests\CardiMapper.Workflow.Domain.Tests.csproj
    - Domain\CardiMapper.Render.Domain.Tests\CardiMapper.Render.Domain.Tests.csproj
    - Domain\CardiMapper.PatientCache.Domain.Tests\CardiMapper.PatientCache.Domain.Tests.csproj
    - Domain\CardiMapper.CatheterCreator.Domain.Tests\CardiMapper.CatheterCreator.Domain.Tests.csproj

## Settings

This section contains settings and data used by execution steps.

### Excluded projects

Table below contains projects that do belong to the dependency graph for selected projects and should not be included in the upgrade.

| Project name | Description |
|:-------------|:-----------:|
| (none)       | N/A         |

### Aggregate NuGet packages modifications across all projects

NuGet packages used across all selected projects or their dependencies that need version update in projects that reference them.

| Package Name                              | Current Version | New Version | Description                           |
|:------------------------------------------|:---------------:|:-----------:|:--------------------------------------|
| Microsoft.Extensions.Configuration.Binder | 8.0.2           | 10.0.8      | Recommended for .NET 10.0             |
| Microsoft.Extensions.Configuration.Json   | 8.0.1           | 10.0.8      | Recommended for .NET 10.0             |
| Microsoft.Extensions.Hosting              | 8.0.1           | 10.0.8      | Recommended for .NET 10.0             |
| System.Runtime.Caching                    | 8.0.1           | 10.0.8      | Recommended for .NET 10.0             |

**Note:** The following packages will keep their current versions (no downgrade):
- DotNetProjects.WpfToolkit.Input: 6.1.94 (keep)
- gong-wpf-dragdrop: 4.0.0 (keep)
- MaterialDesignColors: 5.3.0 (keep)
- MaterialDesignThemes: 5.3.0 (keep)
- Microsoft.Xaml.Behaviors.Wpf: 1.1.135 (keep)

### Project upgrade details

This section contains details about each project upgrade and modifications that need to be done in the project.

#### CardiMapper.Interfaces modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Infrastructure modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Hosting should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Resources modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Toolkit modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.CatheterCreator.Domain modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### Cardimapper.Piu.Domain modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Anatomy.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Render.Domain modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Notes.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Workflow.Domain modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Render.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Signals.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Chalkboard.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.QuickStatus.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.PatientCache.Domain modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- System.Runtime.Caching should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Status.Module.Infrastructure modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Status.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Workflow.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.CatheterCreator.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Peripherals.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Log.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Magnetics.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Status.Front.Panel.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Status.Collection.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### CardiMapper.Status.Board.Module modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### 1_CardiMapper.App modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

NuGet packages changes:
- Microsoft.Extensions.Configuration.Binder should be updated from `8.0.2` to `10.0.8` (*recommended for .NET 10.0*)
- Microsoft.Extensions.Configuration.Json should be updated from `8.0.1` to `10.0.8` (*recommended for .NET 10.0*)

#### 1_CardiMapper.App.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.QuickStatus.Module.Test modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Signals.Module.Test modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### Cardimapper.Piu.Domain.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.Workflow.Domain.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0` to `net10.0`

#### CardiMapper.Render.Domain.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.PatientCache.Domain.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

#### CardiMapper.CatheterCreator.Domain.Tests modifications

Project properties changes:
- Target framework should be changed from `net8.0-windows` to `net10.0-windows`

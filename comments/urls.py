from . import views
from django.urls import path


app_name = 'comments'

urlpatterns = [
    path('group/<int:group_id>/post/<int:post_id>/create/', views.create, name='create'),
    path('delete/<int:group_id>/<int:post_id>/<int:comment_id>', views.delete, name='delete'),
]
